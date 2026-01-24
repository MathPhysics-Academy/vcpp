/*
 *  vcpp.wgpu.web - WebGPU rendering backend for browsers (Emscripten)
 *
 *  Uses browser's native WebGPU API via Emscripten bindings.
 *  Now uses shared modules for types, mesh generation, and render interface.
 */

module;

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <webgpu/webgpu.h>
#endif

import std;

export module vcpp.wgpu.web;

import vcpp;

#ifdef __EMSCRIPTEN__

namespace vcpp::wgpu::web
{

// ============================================================================
// Use shared types from render modules
// ============================================================================

using namespace vcpp::render;
using vcpp::mesh::vertex;

// ============================================================================
// Shader (WGSL) with texture support
// ============================================================================

inline constexpr const char* shader_source = R"(
struct Camera {
  view: mat4x4<f32>,
  proj: mat4x4<f32>,
  view_proj: mat4x4<f32>,
  pos: vec3<f32>,
}

@group(0) @binding(0) var<uniform> camera: Camera;

struct VertexInput {
  @location(0) pos: vec3<f32>,
  @location(1) normal: vec3<f32>,
  @location(2) uv: vec2<f32>,
}

struct InstanceInput {
  @location(3) model_0: vec4<f32>,
  @location(4) model_1: vec4<f32>,
  @location(5) model_2: vec4<f32>,
  @location(6) model_3: vec4<f32>,
  @location(7) color: vec4<f32>,
  @location(8) material: vec4<f32>,
}

struct VertexOutput {
  @builtin(position) clip_pos: vec4<f32>,
  @location(0) world_normal: vec3<f32>,
  @location(1) world_pos: vec3<f32>,
  @location(2) color: vec4<f32>,
  @location(3) uv: vec2<f32>,
  @location(4) material: vec4<f32>,
}

@vertex
fn vs_main(vert: VertexInput, inst: InstanceInput) -> VertexOutput {
  let model = mat4x4<f32>(inst.model_0, inst.model_1, inst.model_2, inst.model_3);
  let world_pos = model * vec4<f32>(vert.pos, 1.0);
  let normal_mat = mat3x3<f32>(model[0].xyz, model[1].xyz, model[2].xyz);
  var out: VertexOutput;
  out.clip_pos = camera.view_proj * world_pos;
  out.world_normal = normalize(normal_mat * vert.normal);
  out.world_pos = world_pos.xyz;
  out.color = inst.color;
  out.uv = vert.uv;
  out.material = inst.material;
  return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
  // Base color from instance (texture sampling would go here)
  var base_color = in.color;

  // Lighting
  let light_dir = normalize(vec3<f32>(1.0, 1.0, 1.0));
  let ambient = 0.3;
  let diffuse = max(dot(in.world_normal, light_dir), 0.0) * 0.7;
  let view_dir = normalize(camera.pos - in.world_pos);
  let reflect_dir = reflect(-light_dir, in.world_normal);
  let shininess = in.material.x;
  let spec = pow(max(dot(view_dir, reflect_dir), 0.0), 32.0) * shininess * 0.5;
  let lighting = ambient + diffuse + spec;

  return vec4<f32>(base_color.rgb * lighting, base_color.a);
}
)";

// ============================================================================
// Renderer State
// ============================================================================

struct renderer_state
{
  WGPUDevice device{nullptr};
  WGPUQueue queue{nullptr};
  WGPUSurface surface{nullptr};
  WGPURenderPipeline pipeline{nullptr};
  WGPUBindGroup bind_group{nullptr};
  WGPUBindGroupLayout bind_group_layout{nullptr};
  WGPUBuffer camera_buffer{nullptr};

  // Instance buffers per type to avoid race conditions
  WGPUBuffer sphere_ib{nullptr};
  std::size_t sphere_ib_cap{0};

  WGPUBuffer ellipsoid_ib{nullptr};
  std::size_t ellipsoid_ib_cap{0};

  WGPUBuffer box_ib{nullptr};
  std::size_t box_ib_cap{0};

  WGPUBuffer cylinder_ib{nullptr};
  std::size_t cylinder_ib_cap{0};

  WGPUBuffer cone_ib{nullptr};
  std::size_t cone_ib_cap{0};

  WGPUBuffer helix_ib{nullptr};
  std::size_t helix_ib_cap{0};

  // Per-curve mesh data (curves have dynamic geometry, can't use standard instancing)
  struct curve_mesh_data
  {
    WGPUBuffer vertex_buffer{nullptr};
    WGPUBuffer index_buffer{nullptr};
    uint32_t index_count{0};
    std::size_t buffer_capacity{0};
  };
  std::vector<curve_mesh_data> curve_meshes;

  WGPUTexture depth_texture{nullptr};
  WGPUTextureView depth_view{nullptr};

  struct mesh_data
  {
    WGPUBuffer vertex_buffer{nullptr};
    WGPUBuffer index_buffer{nullptr};
    uint32_t index_count{0};
  };
  mesh_data meshes[5]; // 0=sphere, 1=box, 2=cylinder, 3=cone, 4=helix

  int width{800};
  int height{600};
  canvas* current_canvas{nullptr};
  bool initialized{false};
  bool should_close{false};
};

inline renderer_state g_renderer{};

// ============================================================================
// Helper Functions
// ============================================================================

inline WGPUBuffer create_buffer(WGPUBufferUsage usage, std::size_t size, const void* data = nullptr)
{
  WGPUBufferDescriptor desc{};
  desc.size = size;
  desc.usage = static_cast<WGPUBufferUsage>(usage);
  desc.mappedAtCreation = data != nullptr;
  WGPUBuffer buffer = wgpuDeviceCreateBuffer(g_renderer.device, &desc);
  if (data && buffer)
  {
    void* mapped = wgpuBufferGetMappedRange(buffer, 0, size);
    if (mapped)
    {
      std::memcpy(mapped, data, size);
      wgpuBufferUnmap(buffer);
    }
  }
  return buffer;
}

inline void create_mesh_buffers(int idx, const mesh::mesh_data& mesh_d)
{
  g_renderer.meshes[idx].vertex_buffer =
    create_buffer(static_cast<WGPUBufferUsage>(WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst),
                  mesh_d.vertices.size() * sizeof(vertex), mesh_d.vertices.data());
  g_renderer.meshes[idx].index_buffer =
    create_buffer(static_cast<WGPUBufferUsage>(WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst),
                  mesh_d.indices.size() * sizeof(uint32_t), mesh_d.indices.data());
  g_renderer.meshes[idx].index_count = static_cast<uint32_t>(mesh_d.indices.size());
}

// ============================================================================
// Render Frame
// ============================================================================

inline void render_frame()
{
  if (!g_renderer.initialized || !g_renderer.current_canvas)
    return;

  // Check for canvas resize
  double cw, ch;
  emscripten_get_element_css_size("#canvas", &cw, &ch);

  // Handle High DPI (Retina)
  double dpr = emscripten_get_device_pixel_ratio();
  int new_width = static_cast<int>(cw * dpr);
  int new_height = static_cast<int>(ch * dpr);

  if (new_width != g_renderer.width || new_height != g_renderer.height)
  {
    if (new_width > 0 && new_height > 0)
    {
      g_renderer.width = new_width;
      g_renderer.height = new_height;

      // Reconfigure Surface
      WGPUSurfaceConfiguration config{};
      config.device = g_renderer.device;
      config.format = WGPUTextureFormat_BGRA8Unorm;
      config.usage = WGPUTextureUsage_RenderAttachment;
      config.width = g_renderer.width;
      config.height = g_renderer.height;
      config.presentMode = WGPUPresentMode_Fifo;
      config.alphaMode = WGPUCompositeAlphaMode_Opaque;
      wgpuSurfaceConfigure(g_renderer.surface, &config);

      // Recreate Depth Texture
      if (g_renderer.depth_view)
        wgpuTextureViewRelease(g_renderer.depth_view);
      if (g_renderer.depth_texture)
        wgpuTextureRelease(g_renderer.depth_texture);

      WGPUTextureDescriptor depth_desc{};
      depth_desc.size = {static_cast<uint32_t>(g_renderer.width), static_cast<uint32_t>(g_renderer.height), 1};
      depth_desc.format = WGPUTextureFormat_Depth24Plus;
      depth_desc.usage = WGPUTextureUsage_RenderAttachment;
      depth_desc.mipLevelCount = 1;
      depth_desc.sampleCount = 1;
      depth_desc.dimension = WGPUTextureDimension_2D;
      g_renderer.depth_texture = wgpuDeviceCreateTexture(g_renderer.device, &depth_desc);
      g_renderer.depth_view = wgpuTextureCreateView(g_renderer.depth_texture, nullptr);

      // Update Canvas Buffer Size (HTML5 API) - important for crisp rendering
      emscripten_set_canvas_element_size("#canvas", g_renderer.width, g_renderer.height);
    }
  }

  canvas& c = *g_renderer.current_canvas;

  WGPUSurfaceTexture surface_texture{};
  wgpuSurfaceGetCurrentTexture(g_renderer.surface, &surface_texture);
  if (surface_texture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
      surface_texture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal)
    return;

  WGPUTextureView back_buffer = wgpuTextureCreateView(surface_texture.texture, nullptr);
  if (!back_buffer)
    return;

  // Update camera uniforms
  camera_uniforms cam{};
  cam.view = matrix::look_at(c.m_camera.m_pos, c.m_camera.m_center, c.m_camera.m_up);
  float fov_rad = static_cast<float>(c.m_camera.m_fov * 3.14159265 / 180.0);
  cam.projection = matrix::perspective(fov_rad, static_cast<float>(g_renderer.width) / g_renderer.height,
                                        static_cast<float>(c.m_camera.m_near), static_cast<float>(c.m_camera.m_far));
  cam.view_projection = matrix::multiply(cam.projection, cam.view);
  cam.camera_pos = to_gpu(c.m_camera.m_pos);
  wgpuQueueWriteBuffer(g_renderer.queue, g_renderer.camera_buffer, 0, &cam, sizeof(cam));

  WGPUCommandEncoderDescriptor enc_desc{};
  WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(g_renderer.device, &enc_desc);

  WGPURenderPassColorAttachment color_att{};
  color_att.view = back_buffer;
  color_att.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
  color_att.loadOp = WGPULoadOp_Clear;
  color_att.storeOp = WGPUStoreOp_Store;
  color_att.clearValue = {c.m_background.x(), c.m_background.y(), c.m_background.z(), 1.0};

  WGPURenderPassDepthStencilAttachment depth_att{};
  depth_att.view = g_renderer.depth_view;
  depth_att.depthLoadOp = WGPULoadOp_Clear;
  depth_att.depthStoreOp = WGPUStoreOp_Store;
  depth_att.depthClearValue = 1.0f;

  WGPURenderPassDescriptor pass_desc{};
  pass_desc.colorAttachmentCount = 1;
  pass_desc.colorAttachments = &color_att;
  pass_desc.depthStencilAttachment = &depth_att;

  WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &pass_desc);
  wgpuRenderPassEncoderSetPipeline(pass, g_renderer.pipeline);
  wgpuRenderPassEncoderSetBindGroup(pass, 0, g_renderer.bind_group, 0, nullptr);

  // Helper to build instance data
  auto build_instance = [](const object_base& obj, const vec3& scale_factors) -> instance_data {
    instance_data inst{};
    inst.model = compute_model_matrix(obj.m_pos, obj.m_axis, obj.m_up, scale_factors);
    inst.color = to_gpu4(obj.m_color, static_cast<float>(obj.m_opacity));
    float has_tex = obj.m_texture.valid() ? 1.0f : 0.0f;
    float tex_idx = obj.m_texture.valid() ? static_cast<float>(obj.m_texture.index) : 0.0f;
    inst.material = {static_cast<float>(obj.m_shininess), obj.m_emissive ? 1.0f : 0.0f, tex_idx, has_tex};
    return inst;
  };

  // Draw spheres
  std::vector<instance_data> sphere_instances;
  for (const auto& s : c.m_spheres)
  {
    if (!s.m_visible)
      continue;
    instance_data inst{};
    gpu_mat4 tr = matrix::translate(static_cast<float>(s.m_pos.x()), static_cast<float>(s.m_pos.y()),
                                    static_cast<float>(s.m_pos.z()));
    float scale = static_cast<float>(s.m_radius * 2);
    inst.model = matrix::multiply(tr, matrix::scale(scale, scale, scale));
    inst.color = to_gpu4(s.m_color, static_cast<float>(s.m_opacity));
    inst.material = {static_cast<float>(s.m_shininess), 0, 0, 0};
    sphere_instances.push_back(inst);
  }

  if (!sphere_instances.empty())
  {
    std::size_t size = sphere_instances.size() * sizeof(instance_data);
    if (size > g_renderer.sphere_ib_cap)
    {
      if (g_renderer.sphere_ib)
        wgpuBufferRelease(g_renderer.sphere_ib);
      g_renderer.sphere_ib_cap = size * 2;
      g_renderer.sphere_ib = create_buffer(
        static_cast<WGPUBufferUsage>(WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst), g_renderer.sphere_ib_cap);
    }
    wgpuQueueWriteBuffer(g_renderer.queue, g_renderer.sphere_ib, 0, sphere_instances.data(), size);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 0, g_renderer.meshes[0].vertex_buffer, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 1, g_renderer.sphere_ib, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetIndexBuffer(pass, g_renderer.meshes[0].index_buffer, WGPUIndexFormat_Uint32, 0,
                                        WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderDrawIndexed(pass, g_renderer.meshes[0].index_count,
                                     static_cast<uint32_t>(sphere_instances.size()), 0, 0, 0);
  }

  // Draw ellipsoids (reuse sphere mesh, index 0)
  std::vector<instance_data> ellipsoid_instances;
  for (const auto& e : c.m_ellipsoids)
  {
    if (!e.m_visible)
      continue;
    ellipsoid_instances.push_back(build_instance(e, vec3{e.m_length, e.m_height, e.m_width}));
  }

  if (!ellipsoid_instances.empty())
  {
    std::size_t size = ellipsoid_instances.size() * sizeof(instance_data);
    if (size > g_renderer.ellipsoid_ib_cap)
    {
      if (g_renderer.ellipsoid_ib)
        wgpuBufferRelease(g_renderer.ellipsoid_ib);
      g_renderer.ellipsoid_ib_cap = size * 2;
      g_renderer.ellipsoid_ib = create_buffer(
        static_cast<WGPUBufferUsage>(WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst), g_renderer.ellipsoid_ib_cap);
    }
    wgpuQueueWriteBuffer(g_renderer.queue, g_renderer.ellipsoid_ib, 0, ellipsoid_instances.data(), size);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 0, g_renderer.meshes[0].vertex_buffer, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 1, g_renderer.ellipsoid_ib, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetIndexBuffer(pass, g_renderer.meshes[0].index_buffer, WGPUIndexFormat_Uint32, 0,
                                        WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderDrawIndexed(pass, g_renderer.meshes[0].index_count,
                                     static_cast<uint32_t>(ellipsoid_instances.size()), 0, 0, 0);
  }

  // Draw boxes
  std::vector<instance_data> box_instances;
  for (const auto& b : c.m_boxes)
  {
    if (!b.m_visible)
      continue;
    instance_data inst{};
    gpu_mat4 tr = matrix::translate(static_cast<float>(b.m_pos.x()), static_cast<float>(b.m_pos.y()),
                                    static_cast<float>(b.m_pos.z()));
    inst.model = matrix::multiply(tr, matrix::scale(static_cast<float>(b.m_length), static_cast<float>(b.m_height),
                                                    static_cast<float>(b.m_width)));
    inst.color = to_gpu4(b.m_color, static_cast<float>(b.m_opacity));
    inst.material = {static_cast<float>(b.m_shininess), 0, 0, 0};
    box_instances.push_back(inst);
  }

  if (!box_instances.empty())
  {
    std::size_t size = box_instances.size() * sizeof(instance_data);
    if (size > g_renderer.box_ib_cap)
    {
      if (g_renderer.box_ib)
        wgpuBufferRelease(g_renderer.box_ib);
      g_renderer.box_ib_cap = size * 2;
      g_renderer.box_ib = create_buffer(static_cast<WGPUBufferUsage>(WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst),
                                        g_renderer.box_ib_cap);
    }
    wgpuQueueWriteBuffer(g_renderer.queue, g_renderer.box_ib, 0, box_instances.data(), size);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 0, g_renderer.meshes[1].vertex_buffer, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 1, g_renderer.box_ib, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetIndexBuffer(pass, g_renderer.meshes[1].index_buffer, WGPUIndexFormat_Uint32, 0,
                                        WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderDrawIndexed(pass, g_renderer.meshes[1].index_count,
                                     static_cast<uint32_t>(box_instances.size()), 0, 0, 0);
  }

  std::vector<instance_data> cylinder_instances;
  std::vector<instance_data> cone_instances;
  std::vector<instance_data> helix_instances;

  // Process m_cylinders
  for (const auto& obj : c.m_cylinders)
  {
    if (!obj.m_visible)
      continue;
    instance_data inst{};
    float len = static_cast<float>(mag(obj.m_axis));
    float dia = static_cast<float>(obj.m_radius * 2.0);
    gpu_mat4 rot = matrix::align_x_to_axis(obj.m_axis);
    gpu_mat4 scale = matrix::scale(len, dia, dia);
    gpu_mat4 tr = matrix::translate(static_cast<float>(obj.m_pos.x()), static_cast<float>(obj.m_pos.y()),
                                    static_cast<float>(obj.m_pos.z()));
    inst.model = matrix::multiply(tr, matrix::multiply(rot, scale));
    inst.color = to_gpu4(obj.m_color, static_cast<float>(obj.m_opacity));
    inst.material = {static_cast<float>(obj.m_shininess), 0, 0, 0};
    cylinder_instances.push_back(inst);
  }

  // Process m_cones
  for (const auto& obj : c.m_cones)
  {
    if (!obj.m_visible)
      continue;
    instance_data inst{};
    float len = static_cast<float>(mag(obj.m_axis));
    float dia = static_cast<float>(obj.m_radius * 2.0);
    gpu_mat4 rot = matrix::align_x_to_axis(obj.m_axis);
    gpu_mat4 scale = matrix::scale(len, dia, dia);
    gpu_mat4 tr = matrix::translate(static_cast<float>(obj.m_pos.x()), static_cast<float>(obj.m_pos.y()),
                                    static_cast<float>(obj.m_pos.z()));
    inst.model = matrix::multiply(tr, matrix::multiply(rot, scale));
    inst.color = to_gpu4(obj.m_color, static_cast<float>(obj.m_opacity));
    inst.material = {static_cast<float>(obj.m_shininess), 0, 0, 0};
    cone_instances.push_back(inst);
  }

  // Process m_arrows (Composite)
  for (const auto& obj : c.m_arrows)
  {
    if (!obj.m_visible)
      continue;

    double len = mag(obj.m_axis);
    if (len < 1e-6)
      continue;

    double shaft_dia = (obj.m_shaftwidth > 0) ? obj.m_shaftwidth : (0.1 * len);
    double head_len = obj.m_headlength;
    double head_dia = obj.m_headwidth;
    if (head_len > len)
      head_len = len;
    double shaft_len = len - head_len;

    gpu_mat4 rot = matrix::align_x_to_axis(obj.m_axis);

    // Shaft (Cylinder)
    if (shaft_len > 0)
    {
      instance_data inst{};
      gpu_mat4 scale = matrix::scale(static_cast<float>(shaft_len), static_cast<float>(shaft_dia),
                                     static_cast<float>(shaft_dia));
      gpu_mat4 tr = matrix::translate(static_cast<float>(obj.m_pos.x()), static_cast<float>(obj.m_pos.y()),
                                      static_cast<float>(obj.m_pos.z()));
      inst.model = matrix::multiply(tr, matrix::multiply(rot, scale));
      inst.color = to_gpu4(obj.m_color, static_cast<float>(obj.m_opacity));
      inst.material = {static_cast<float>(obj.m_shininess), 0, 0, 0};
      cylinder_instances.push_back(inst);
    }

    // Head (Cone)
    {
      instance_data inst{};
      vec3 head_pos = obj.m_pos + hat(obj.m_axis) * shaft_len;
      gpu_mat4 scale = matrix::scale(static_cast<float>(head_len), static_cast<float>(head_dia),
                                     static_cast<float>(head_dia));
      gpu_mat4 tr = matrix::translate(static_cast<float>(head_pos.x()), static_cast<float>(head_pos.y()),
                                      static_cast<float>(head_pos.z()));
      inst.model = matrix::multiply(tr, matrix::multiply(rot, scale));
      inst.color = to_gpu4(obj.m_color, static_cast<float>(obj.m_opacity));
      inst.material = {static_cast<float>(obj.m_shininess), 0, 0, 0};
      cone_instances.push_back(inst);
    }
  }

  // Process m_helixes
  for (const auto& h : c.m_helixes)
  {
    if (!h.m_visible)
      continue;

    double len = mag(h.m_axis);
    if (len < 1e-6)
      continue;

    instance_data inst{};
    gpu_mat4 rot = matrix::align_x_to_axis(h.m_axis);
    gpu_mat4 scale = matrix::scale(static_cast<float>(len), static_cast<float>(h.m_radius),
                                   static_cast<float>(h.m_radius));
    gpu_mat4 tr = matrix::translate(static_cast<float>(h.m_pos.x()), static_cast<float>(h.m_pos.y()),
                                    static_cast<float>(h.m_pos.z()));
    inst.model = matrix::multiply(tr, matrix::multiply(rot, scale));
    inst.color = to_gpu4(h.m_color, static_cast<float>(h.m_opacity));
    inst.material = {static_cast<float>(h.m_shininess), 0, 0, 0};
    helix_instances.push_back(inst);
  }

  // Draw Cylinders
  if (!cylinder_instances.empty())
  {
    std::size_t size = cylinder_instances.size() * sizeof(instance_data);
    if (size > g_renderer.cylinder_ib_cap)
    {
      if (g_renderer.cylinder_ib)
        wgpuBufferRelease(g_renderer.cylinder_ib);
      g_renderer.cylinder_ib_cap = size * 2;
      g_renderer.cylinder_ib = create_buffer(
        static_cast<WGPUBufferUsage>(WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst), g_renderer.cylinder_ib_cap);
    }
    wgpuQueueWriteBuffer(g_renderer.queue, g_renderer.cylinder_ib, 0, cylinder_instances.data(), size);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 0, g_renderer.meshes[2].vertex_buffer, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 1, g_renderer.cylinder_ib, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetIndexBuffer(pass, g_renderer.meshes[2].index_buffer, WGPUIndexFormat_Uint32, 0,
                                        WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderDrawIndexed(pass, g_renderer.meshes[2].index_count,
                                     static_cast<uint32_t>(cylinder_instances.size()), 0, 0, 0);
  }

  // Draw Cones
  if (!cone_instances.empty())
  {
    std::size_t size = cone_instances.size() * sizeof(instance_data);
    if (size > g_renderer.cone_ib_cap)
    {
      if (g_renderer.cone_ib)
        wgpuBufferRelease(g_renderer.cone_ib);
      g_renderer.cone_ib_cap = size * 2;
      g_renderer.cone_ib = create_buffer(
        static_cast<WGPUBufferUsage>(WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst), g_renderer.cone_ib_cap);
    }
    wgpuQueueWriteBuffer(g_renderer.queue, g_renderer.cone_ib, 0, cone_instances.data(), size);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 0, g_renderer.meshes[3].vertex_buffer, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 1, g_renderer.cone_ib, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetIndexBuffer(pass, g_renderer.meshes[3].index_buffer, WGPUIndexFormat_Uint32, 0,
                                        WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderDrawIndexed(pass, g_renderer.meshes[3].index_count,
                                     static_cast<uint32_t>(cone_instances.size()), 0, 0, 0);
  }

  // Draw Helices
  if (!helix_instances.empty())
  {
    std::size_t size = helix_instances.size() * sizeof(instance_data);
    if (size > g_renderer.helix_ib_cap)
    {
      if (g_renderer.helix_ib)
        wgpuBufferRelease(g_renderer.helix_ib);
      g_renderer.helix_ib_cap = size * 2;
      g_renderer.helix_ib = create_buffer(
        static_cast<WGPUBufferUsage>(WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst), g_renderer.helix_ib_cap);
    }
    wgpuQueueWriteBuffer(g_renderer.queue, g_renderer.helix_ib, 0, helix_instances.data(), size);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 0, g_renderer.meshes[4].vertex_buffer, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 1, g_renderer.helix_ib, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetIndexBuffer(pass, g_renderer.meshes[4].index_buffer, WGPUIndexFormat_Uint32, 0,
                                        WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderDrawIndexed(pass, g_renderer.meshes[4].index_count,
                                     static_cast<uint32_t>(helix_instances.size()), 0, 0, 0);
  }

  // Draw Curves (per-curve dynamic mesh generation)
  while (g_renderer.curve_meshes.size() < c.m_curves.size())
    g_renderer.curve_meshes.push_back({});

  for (std::size_t curve_idx = 0; curve_idx < c.m_curves.size(); ++curve_idx)
  {
    const auto& crv = c.m_curves[curve_idx];
    if (!crv.m_visible || crv.m_points.size() < 2)
      continue;

    auto& curve_mesh = g_renderer.curve_meshes[curve_idx];

    // Regenerate mesh if geometry is dirty
    if (crv.m_geometry_dirty)
    {
      auto tube_mesh = mesh::generate_tube(crv.m_points, static_cast<float>(crv.m_radius), 8);

      if (!tube_mesh.indices.empty())
      {
        std::size_t vb_size = tube_mesh.vertices.size() * sizeof(vertex);
        std::size_t ib_size = tube_mesh.indices.size() * sizeof(uint32_t);
        std::size_t total_size = vb_size + ib_size;

        // Recreate buffers if capacity insufficient
        if (total_size > curve_mesh.buffer_capacity)
        {
          if (curve_mesh.vertex_buffer)
            wgpuBufferRelease(curve_mesh.vertex_buffer);
          if (curve_mesh.index_buffer)
            wgpuBufferRelease(curve_mesh.index_buffer);

          curve_mesh.buffer_capacity = total_size * 2;
          curve_mesh.vertex_buffer = create_buffer(
            static_cast<WGPUBufferUsage>(WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst), vb_size * 2);
          curve_mesh.index_buffer = create_buffer(
            static_cast<WGPUBufferUsage>(WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst), ib_size * 2);
        }

        wgpuQueueWriteBuffer(g_renderer.queue, curve_mesh.vertex_buffer, 0, tube_mesh.vertices.data(), vb_size);
        wgpuQueueWriteBuffer(g_renderer.queue, curve_mesh.index_buffer, 0, tube_mesh.indices.data(), ib_size);
        curve_mesh.index_count = static_cast<uint32_t>(tube_mesh.indices.size());
      }

      crv.m_geometry_dirty = false;
    }

    if (curve_mesh.index_count == 0 || !curve_mesh.vertex_buffer || !curve_mesh.index_buffer)
      continue;

    // Create instance data for this curve (identity transform + curve color)
    instance_data inst{};
    inst.model = matrix::identity();
    inst.color = to_gpu4(crv.m_color, static_cast<float>(crv.m_opacity));
    inst.material = {static_cast<float>(crv.m_shininess), 0, 0, 0};

    // Use a temporary instance buffer
    static WGPUBuffer curve_single_ib = nullptr;
    static std::size_t curve_single_ib_cap = 0;
    std::size_t inst_size = sizeof(instance_data);

    if (inst_size > curve_single_ib_cap)
    {
      if (curve_single_ib)
        wgpuBufferRelease(curve_single_ib);
      curve_single_ib_cap = inst_size * 2;
      curve_single_ib = create_buffer(
        static_cast<WGPUBufferUsage>(WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst), curve_single_ib_cap);
    }
    wgpuQueueWriteBuffer(g_renderer.queue, curve_single_ib, 0, &inst, inst_size);

    wgpuRenderPassEncoderSetVertexBuffer(pass, 0, curve_mesh.vertex_buffer, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 1, curve_single_ib, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetIndexBuffer(pass, curve_mesh.index_buffer, WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderDrawIndexed(pass, curve_mesh.index_count, 1, 0, 0, 0);
  }

  wgpuRenderPassEncoderEnd(pass);
  wgpuRenderPassEncoderRelease(pass);

  WGPUCommandBufferDescriptor cmd_desc{};
  WGPUCommandBuffer commands = wgpuCommandEncoderFinish(encoder, &cmd_desc);
  wgpuQueueSubmit(g_renderer.queue, 1, &commands);
  wgpuCommandBufferRelease(commands);
  wgpuCommandEncoderRelease(encoder);
  wgpuTextureViewRelease(back_buffer);
}

// ============================================================================
// Input Callbacks (Emscripten HTML5 API)
// ============================================================================

inline EM_BOOL on_mouse_move(int eventType, const EmscriptenMouseEvent* e, void* userData)
{
  (void)eventType;
  (void)userData;
  vcpp::g_input.mouse.x = e->targetX;
  vcpp::g_input.mouse.y = e->targetY;
  vcpp::g_input.ctrl_held = e->ctrlKey;
  vcpp::g_input.shift_held = e->shiftKey;
  vcpp::g_input.alt_held = e->altKey;
  return EM_TRUE;
}

inline EM_BOOL on_mouse_down(int eventType, const EmscriptenMouseEvent* e, void* userData)
{
  (void)eventType;
  (void)userData;
  vcpp::g_input.mouse.x = e->targetX;
  vcpp::g_input.mouse.y = e->targetY;
  vcpp::g_input.mouse.last_x = e->targetX;
  vcpp::g_input.mouse.last_y = e->targetY;
  vcpp::g_input.ctrl_held = e->ctrlKey;
  vcpp::g_input.shift_held = e->shiftKey;
  vcpp::g_input.alt_held = e->altKey;
  switch (e->button)
  {
    case 0:
      vcpp::g_input.mouse.left_down = true;
      break;
    case 1:
      vcpp::g_input.mouse.middle_down = true;
      break;
    case 2:
      vcpp::g_input.mouse.right_down = true;
      break;
  }
  return EM_TRUE;
}

inline EM_BOOL on_mouse_up(int eventType, const EmscriptenMouseEvent* e, void* userData)
{
  (void)eventType;
  (void)userData;
  switch (e->button)
  {
    case 0:
      vcpp::g_input.mouse.left_down = false;
      break;
    case 1:
      vcpp::g_input.mouse.middle_down = false;
      break;
    case 2:
      vcpp::g_input.mouse.right_down = false;
      break;
  }
  return EM_TRUE;
}

inline EM_BOOL on_wheel(int eventType, const EmscriptenWheelEvent* e, void* userData)
{
  (void)eventType;
  (void)userData;
  vcpp::g_input.mouse.scroll_delta += e->deltaY * 0.01;
  return EM_TRUE;
}

inline EM_BOOL on_key_down(int eventType, const EmscriptenKeyboardEvent* e, void* userData)
{
  (void)eventType;
  (void)userData;
  vcpp::g_input.key_down_events.push_back(e->code);
  if (e->metaKey || e->ctrlKey)
  {
    return EM_FALSE;
  }
  return EM_TRUE;
}

// ============================================================================
// Main Loop Callback
// ============================================================================

inline void (*user_update_fn)() = nullptr;

inline void main_loop_callback()
{
  if (g_renderer.current_canvas)
  {
    vcpp::process_camera_input(*g_renderer.current_canvas);
  }
  if (user_update_fn)
    user_update_fn();

  vcpp::g_input.key_down_events.clear();
  vcpp::graph_bridge::flush_updates();
  render_frame();
}

// ============================================================================
// Public API
// ============================================================================

export inline bool init(canvas& c, const char* canvas_selector = "#canvas")
{
  g_renderer.device = emscripten_webgpu_get_device();
  if (!g_renderer.device)
    return false;

  g_renderer.queue = wgpuDeviceGetQueue(g_renderer.device);

  double w, h;
  emscripten_get_element_css_size(canvas_selector, &w, &h);
  g_renderer.width = static_cast<int>(w);
  g_renderer.height = static_cast<int>(h);

  WGPUEmscriptenSurfaceSourceCanvasHTMLSelector canvas_desc{};
  canvas_desc.chain.sType = WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector;
  canvas_desc.selector = {canvas_selector, WGPU_STRLEN};
  WGPUSurfaceDescriptor surface_desc{};
  surface_desc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&canvas_desc);

  WGPUInstance instance = wgpuCreateInstance(nullptr);
  g_renderer.surface = wgpuInstanceCreateSurface(instance, &surface_desc);

  WGPUSurfaceConfiguration config{};
  config.device = g_renderer.device;
  config.format = WGPUTextureFormat_BGRA8Unorm;
  config.usage = WGPUTextureUsage_RenderAttachment;
  config.width = g_renderer.width;
  config.height = g_renderer.height;
  config.presentMode = WGPUPresentMode_Fifo;
  config.alphaMode = WGPUCompositeAlphaMode_Opaque;
  wgpuSurfaceConfigure(g_renderer.surface, &config);

  WGPUTextureDescriptor depth_desc{};
  depth_desc.size = {static_cast<uint32_t>(g_renderer.width), static_cast<uint32_t>(g_renderer.height), 1};
  depth_desc.format = WGPUTextureFormat_Depth24Plus;
  depth_desc.usage = WGPUTextureUsage_RenderAttachment;
  depth_desc.mipLevelCount = 1;
  depth_desc.sampleCount = 1;
  depth_desc.dimension = WGPUTextureDimension_2D;
  g_renderer.depth_texture = wgpuDeviceCreateTexture(g_renderer.device, &depth_desc);
  g_renderer.depth_view = wgpuTextureCreateView(g_renderer.depth_texture, nullptr);

  g_renderer.camera_buffer = create_buffer(
    static_cast<WGPUBufferUsage>(WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst), sizeof(camera_uniforms));

  WGPUBindGroupLayoutEntry bgl_entry{};
  bgl_entry.binding = 0;
  bgl_entry.visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
  bgl_entry.buffer.type = WGPUBufferBindingType_Uniform;
  bgl_entry.buffer.minBindingSize = sizeof(camera_uniforms);

  WGPUBindGroupLayoutDescriptor bgl_desc{};
  bgl_desc.entryCount = 1;
  bgl_desc.entries = &bgl_entry;
  g_renderer.bind_group_layout = wgpuDeviceCreateBindGroupLayout(g_renderer.device, &bgl_desc);

  WGPUBindGroupEntry bg_entry{};
  bg_entry.binding = 0;
  bg_entry.buffer = g_renderer.camera_buffer;
  bg_entry.size = sizeof(camera_uniforms);

  WGPUBindGroupDescriptor bg_desc{};
  bg_desc.layout = g_renderer.bind_group_layout;
  bg_desc.entryCount = 1;
  bg_desc.entries = &bg_entry;
  g_renderer.bind_group = wgpuDeviceCreateBindGroup(g_renderer.device, &bg_desc);

  WGPUShaderSourceWGSL wgsl_desc{};
  wgsl_desc.chain.sType = WGPUSType_ShaderSourceWGSL;
  wgsl_desc.code = {shader_source, WGPU_STRLEN};

  WGPUShaderModuleDescriptor sm_desc{};
  sm_desc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&wgsl_desc);
  WGPUShaderModule shader = wgpuDeviceCreateShaderModule(g_renderer.device, &sm_desc);

  WGPUPipelineLayoutDescriptor pl_desc{};
  pl_desc.bindGroupLayoutCount = 1;
  pl_desc.bindGroupLayouts = &g_renderer.bind_group_layout;
  WGPUPipelineLayout pipeline_layout = wgpuDeviceCreatePipelineLayout(g_renderer.device, &pl_desc);

  WGPUVertexAttribute vertex_attrs[] = {
    {nullptr, WGPUVertexFormat_Float32x3, 0, 0},  // pos
    {nullptr, WGPUVertexFormat_Float32x3, 12, 1}, // normal
    {nullptr, WGPUVertexFormat_Float32x2, 24, 2}, // uv
  };

  WGPUVertexAttribute instance_attrs[] = {
    {nullptr, WGPUVertexFormat_Float32x4, 0, 3},  // model col 0
    {nullptr, WGPUVertexFormat_Float32x4, 16, 4}, // model col 1
    {nullptr, WGPUVertexFormat_Float32x4, 32, 5}, // model col 2
    {nullptr, WGPUVertexFormat_Float32x4, 48, 6}, // model col 3
    {nullptr, WGPUVertexFormat_Float32x4, 64, 7}, // color
    {nullptr, WGPUVertexFormat_Float32x4, 80, 8}, // material
  };

  WGPUVertexBufferLayout vb_layouts[2] = {};
  vb_layouts[0].arrayStride = sizeof(vertex);
  vb_layouts[0].stepMode = WGPUVertexStepMode_Vertex;
  vb_layouts[0].attributeCount = 3;
  vb_layouts[0].attributes = vertex_attrs;

  vb_layouts[1].arrayStride = sizeof(instance_data);
  vb_layouts[1].stepMode = WGPUVertexStepMode_Instance;
  vb_layouts[1].attributeCount = 6;
  vb_layouts[1].attributes = instance_attrs;

  WGPUColorTargetState color_target{};
  color_target.format = WGPUTextureFormat_BGRA8Unorm;
  color_target.writeMask = WGPUColorWriteMask_All;

  WGPUFragmentState frag_state{};
  frag_state.module = shader;
  frag_state.entryPoint = {"fs_main", WGPU_STRLEN};
  frag_state.targetCount = 1;
  frag_state.targets = &color_target;

  WGPUDepthStencilState depth_state{};
  depth_state.format = WGPUTextureFormat_Depth24Plus;
  depth_state.depthWriteEnabled = WGPUOptionalBool_True;
  depth_state.depthCompare = WGPUCompareFunction_Less;

  WGPURenderPipelineDescriptor rp_desc{};
  rp_desc.layout = pipeline_layout;
  rp_desc.vertex.module = shader;
  rp_desc.vertex.entryPoint = {"vs_main", WGPU_STRLEN};
  rp_desc.vertex.bufferCount = 2;
  rp_desc.vertex.buffers = vb_layouts;
  rp_desc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
  rp_desc.primitive.cullMode = WGPUCullMode_None;
  rp_desc.primitive.frontFace = WGPUFrontFace_CCW;
  rp_desc.depthStencil = &depth_state;
  rp_desc.fragment = &frag_state;
  rp_desc.multisample.count = 1;
  rp_desc.multisample.mask = ~0u;

  g_renderer.pipeline = wgpuDeviceCreateRenderPipeline(g_renderer.device, &rp_desc);

  wgpuShaderModuleRelease(shader);
  wgpuPipelineLayoutRelease(pipeline_layout);

  // Create mesh buffers using shared mesh module
  create_mesh_buffers(0, mesh::generate_sphere());
  create_mesh_buffers(1, mesh::generate_box());
  create_mesh_buffers(2, mesh::generate_cylinder());
  create_mesh_buffers(3, mesh::generate_cone());
  create_mesh_buffers(4, mesh::generate_helix());

  // Register input callbacks
  emscripten_set_mousemove_callback(canvas_selector, nullptr, EM_TRUE, on_mouse_move);
  emscripten_set_mousedown_callback(canvas_selector, nullptr, EM_TRUE, on_mouse_down);
  emscripten_set_mouseup_callback(canvas_selector, nullptr, EM_TRUE, on_mouse_up);
  emscripten_set_wheel_callback(canvas_selector, nullptr, EM_TRUE, on_wheel);
  emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_FALSE, on_key_down);
  emscripten_set_click_callback(canvas_selector, nullptr, EM_TRUE,
                                [](int, const EmscriptenMouseEvent*, void*) -> EM_BOOL { return EM_TRUE; });

  g_renderer.current_canvas = &c;
  g_renderer.initialized = true;

  return true;
}

export inline void run(void (*update_fn)())
{
  user_update_fn = update_fn;
  emscripten_set_main_loop(main_loop_callback, 0, true);
}

export inline void shutdown() { g_renderer.should_close = true; }

} // namespace vcpp::wgpu::web

#else
// Stub for non-Emscripten builds
namespace vcpp::wgpu::web
{
export inline bool init(canvas&, const char* = "#canvas") { return false; }
export inline void run(void (*)()) {}
export inline void shutdown() {}
} // namespace vcpp::wgpu::web
#endif
