/*
 *  vcpp.wgpu - WebGPU rendering backend
 *
 *  Uses wgpu-native with the new WebGPU API (no SwapChain, uses Surface).
 *  Now uses shared modules for types, mesh generation, and render interface.
 *
 *  Implements renderer_base for backend-agnostic rendering.
 */

module;

#include <webgpu/webgpu.h>

#include <GLFW/glfw3.h>
#if defined(__APPLE__)
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>
extern "C" void* vcpp_create_metal_layer(void* ns_window);
#elif defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#else
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#endif

import std;

export module vcpp.wgpu;

import vcpp;

namespace vcpp::wgpu
{

// ============================================================================
// Use shared types from render modules
// ============================================================================

using namespace vcpp::render;
using vcpp::mesh::vertex;

// ============================================================================
// Internal Resource Wrappers
// ============================================================================

struct wgpu_texture_resource
{
  WGPUTexture texture{nullptr};
  WGPUTextureView view{nullptr};
  texture_desc desc{};
};

struct wgpu_sampler_resource
{
  WGPUSampler sampler{nullptr};
  sampler_desc desc{};
};

// ============================================================================
// WGSL Shader with UV support and texture sampling
// ============================================================================

inline constexpr const char* shader_source = R"(
struct CameraUniforms {
    view: mat4x4<f32>,
    projection: mat4x4<f32>,
    view_projection: mat4x4<f32>,
    camera_pos: vec3<f32>,
};

@group(0) @binding(0) var<uniform> camera: CameraUniforms;
@group(0) @binding(1) var tex_sampler: sampler;
@group(0) @binding(2) var base_texture: texture_2d<f32>;

struct VertexInput {
    @location(0) position: vec3<f32>,
    @location(1) normal: vec3<f32>,
    @location(2) uv: vec2<f32>,
};

struct InstanceInput {
    @location(3) model_0: vec4<f32>,
    @location(4) model_1: vec4<f32>,
    @location(5) model_2: vec4<f32>,
    @location(6) model_3: vec4<f32>,
    @location(7) color: vec4<f32>,
    @location(8) material: vec4<f32>,
};

struct VertexOutput {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) world_normal: vec3<f32>,
    @location(1) world_pos: vec3<f32>,
    @location(2) color: vec4<f32>,
    @location(3) uv: vec2<f32>,
    @location(4) material: vec4<f32>,
};

@vertex
fn vs_main(vertex: VertexInput, instance: InstanceInput) -> VertexOutput {
    let model = mat4x4<f32>(instance.model_0, instance.model_1, instance.model_2, instance.model_3);
    let world_pos = model * vec4<f32>(vertex.position, 1.0);
    let normal_mat = mat3x3<f32>(model[0].xyz, model[1].xyz, model[2].xyz);
    var out: VertexOutput;
    out.clip_position = camera.view_projection * world_pos;
    out.world_normal = normalize(normal_mat * vertex.normal);
    out.world_pos = world_pos.xyz;
    out.color = instance.color;
    out.uv = vertex.uv;
    out.material = instance.material;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    // Base color from instance
    var base_color = in.color;

    // Sample texture if has_texture flag is set (material.w > 0.5)
    let has_texture = in.material.w > 0.5;
    if (has_texture) {
        let tex_color = textureSample(base_texture, tex_sampler, in.uv);
        base_color = base_color * tex_color;
    }

    // Lighting
    let light_dir = normalize(vec3<f32>(1.0, 1.0, 1.0));
    let ambient = 0.3;
    let diffuse = max(dot(in.world_normal, light_dir), 0.0) * 0.7;
    let view_dir = normalize(camera.camera_pos - in.world_pos);
    let reflect_dir = reflect(-light_dir, in.world_normal);
    let shininess = in.material.x;
    let spec = pow(max(dot(view_dir, reflect_dir), 0.0), 32.0) * shininess * 0.5;
    let lighting = ambient + diffuse + spec;

    return vec4<f32>(base_color.rgb * lighting, base_color.a);
}
)";

// ============================================================================
// Mesh Buffers
// ============================================================================

struct mesh_buffers
{
  WGPUBuffer vertex_buffer{nullptr};
  WGPUBuffer index_buffer{nullptr};
  uint32_t index_count{0};
};

// Forward declaration for GLFW callbacks
class wgpu_renderer;
inline wgpu_renderer* g_wgpu_renderer{nullptr};

// ============================================================================
// wgpu_renderer - WebGPU implementation of renderer_base
// ============================================================================

export class wgpu_renderer : public renderer_base
{
public:
  wgpu_renderer() = default;
  ~wgpu_renderer() override { if (m_initialized) shutdown(); }

  // Non-copyable
  wgpu_renderer(const wgpu_renderer&) = delete;
  wgpu_renderer& operator=(const wgpu_renderer&) = delete;

  // -------------------------------------------------------------------------
  // renderer_base interface implementation
  // -------------------------------------------------------------------------

  bool init(canvas& c) override;
  void shutdown() override;
  bool begin_frame(canvas& c) override;
  void end_frame() override;
  void render(canvas& c) override;

  texture_handle create_texture(const texture_desc& desc) override;
  void upload_texture(texture_handle h, const image_data& data) override;
  void destroy_texture(texture_handle h) override;

  sampler_handle create_sampler(const sampler_desc& desc) override;
  void destroy_sampler(sampler_handle h) override;

  // -------------------------------------------------------------------------
  // WebGPU-specific accessors
  // -------------------------------------------------------------------------

  GLFWwindow* window() const { return m_window; }
  WGPUDevice device() const { return m_device; }
  WGPUQueue queue() const { return m_queue; }

private:
  // Helper functions
  WGPUBuffer create_wgpu_buffer(WGPUBufferUsage usage, std::size_t size, const void* data = nullptr);
  void create_depth_buffer(int w, int h);
  void configure_surface();
  void create_default_texture();
  void create_mesh_buffers(int idx, const mesh::mesh_data& mesh);
  void update_camera_uniforms(const canvas& c);

  // -------------------------------------------------------------------------
  // WebGPU handles
  // -------------------------------------------------------------------------

  GLFWwindow* m_window{nullptr};
  WGPUInstance m_instance{nullptr};
  WGPUSurface m_surface{nullptr};
  WGPUAdapter m_adapter{nullptr};
  WGPUDevice m_device{nullptr};
  WGPUQueue m_queue{nullptr};
  WGPUTextureFormat m_surface_format{WGPUTextureFormat_BGRA8Unorm};
  WGPUTexture m_depth_texture{nullptr};
  WGPUTextureView m_depth_view{nullptr};
  WGPURenderPipeline m_pipeline{nullptr};
  WGPUBindGroupLayout m_bind_group_layout{nullptr};
  WGPUBindGroup m_bind_group{nullptr};
  WGPUBuffer m_camera_buffer{nullptr};

  // Default texture and sampler (used when no texture is bound)
  WGPUTexture m_default_texture{nullptr};
  WGPUTextureView m_default_texture_view{nullptr};
  WGPUSampler m_default_sampler{nullptr};

  // Mesh buffers: 0=sphere, 1=box, 2=cylinder, 3=cone, 4=helix
  std::array<mesh_buffers, 8> m_meshes;

  // Instance buffers
  WGPUBuffer m_sphere_ib{nullptr};
  std::size_t m_sphere_ib_cap{0};
  WGPUBuffer m_ellipsoid_ib{nullptr};
  std::size_t m_ellipsoid_ib_cap{0};
  WGPUBuffer m_box_ib{nullptr};
  std::size_t m_box_ib_cap{0};
  WGPUBuffer m_cylinder_ib{nullptr};
  std::size_t m_cylinder_ib_cap{0};
  WGPUBuffer m_cone_ib{nullptr};
  std::size_t m_cone_ib_cap{0};
  WGPUBuffer m_helix_ib{nullptr};
  std::size_t m_helix_ib_cap{0};

  // Resource pools
  resource_pool<wgpu_texture_resource, texture_handle> m_texture_pool;
  resource_pool<wgpu_sampler_resource, sampler_handle> m_sampler_pool;

  // Frame state
  canvas* m_current_canvas{nullptr};
  WGPUSurfaceTexture m_current_surface_texture{};
  WGPUTextureView m_current_back_buffer{nullptr};
  WGPUCommandEncoder m_current_encoder{nullptr};
  WGPURenderPassEncoder m_current_pass{nullptr};
};

// ============================================================================
// wgpu_renderer Helper Methods
// ============================================================================

inline WGPUBuffer wgpu_renderer::create_wgpu_buffer(WGPUBufferUsage usage, std::size_t size, const void* data)
{
  WGPUBufferDescriptor desc{};
  desc.usage = usage;
  desc.size = size;
  desc.mappedAtCreation = data != nullptr;
  WGPUBuffer buffer = wgpuDeviceCreateBuffer(m_device, &desc);
  if (data)
  {
    void* mapped = wgpuBufferGetMappedRange(buffer, 0, size);
    std::memcpy(mapped, data, size);
    wgpuBufferUnmap(buffer);
  }
  return buffer;
}

inline void wgpu_renderer::create_depth_buffer(int w, int h)
{
  if (m_depth_view)
    wgpuTextureViewRelease(m_depth_view);
  if (m_depth_texture)
    wgpuTextureRelease(m_depth_texture);
  WGPUTextureDescriptor desc{};
  desc.size = {static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1};
  desc.mipLevelCount = 1;
  desc.sampleCount = 1;
  desc.dimension = WGPUTextureDimension_2D;
  desc.format = WGPUTextureFormat_Depth24Plus;
  desc.usage = WGPUTextureUsage_RenderAttachment;
  m_depth_texture = wgpuDeviceCreateTexture(m_device, &desc);
  WGPUTextureViewDescriptor vdesc{};
  vdesc.format = WGPUTextureFormat_Depth24Plus;
  vdesc.dimension = WGPUTextureViewDimension_2D;
  vdesc.mipLevelCount = 1;
  vdesc.arrayLayerCount = 1;
  m_depth_view = wgpuTextureCreateView(m_depth_texture, &vdesc);
}

inline void wgpu_renderer::configure_surface()
{
  WGPUSurfaceConfiguration config{};
  config.device = m_device;
  config.format = m_surface_format;
  config.usage = WGPUTextureUsage_RenderAttachment;
  config.width = m_width;
  config.height = m_height;
  config.presentMode = WGPUPresentMode_Fifo;
  config.alphaMode = WGPUCompositeAlphaMode_Auto;
  wgpuSurfaceConfigure(m_surface, &config);
}

inline void wgpu_renderer::create_default_texture()
{
  // Create a 1x1 white texture
  WGPUTextureDescriptor tex_desc{};
  tex_desc.size = {1, 1, 1};
  tex_desc.mipLevelCount = 1;
  tex_desc.sampleCount = 1;
  tex_desc.dimension = WGPUTextureDimension_2D;
  tex_desc.format = WGPUTextureFormat_RGBA8Unorm;
  tex_desc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
  m_default_texture = wgpuDeviceCreateTexture(m_device, &tex_desc);

  // Upload white pixel
  uint8_t white_pixel[4] = {255, 255, 255, 255};
  WGPUImageCopyTexture dst{};
  dst.texture = m_default_texture;
  dst.mipLevel = 0;
  dst.origin = {0, 0, 0};
  dst.aspect = WGPUTextureAspect_All;
  WGPUTextureDataLayout layout{};
  layout.offset = 0;
  layout.bytesPerRow = 4;
  layout.rowsPerImage = 1;
  WGPUExtent3D extent = {1, 1, 1};
  wgpuQueueWriteTexture(m_queue, &dst, white_pixel, 4, &layout, &extent);

  // Create view
  m_default_texture_view = wgpuTextureCreateView(m_default_texture, nullptr);

  // Create sampler
  WGPUSamplerDescriptor sampler_desc{};
  sampler_desc.magFilter = WGPUFilterMode_Linear;
  sampler_desc.minFilter = WGPUFilterMode_Linear;
  sampler_desc.mipmapFilter = WGPUMipmapFilterMode_Linear;
  sampler_desc.addressModeU = WGPUAddressMode_Repeat;
  sampler_desc.addressModeV = WGPUAddressMode_Repeat;
  sampler_desc.addressModeW = WGPUAddressMode_Repeat;
  sampler_desc.maxAnisotropy = 1;
  m_default_sampler = wgpuDeviceCreateSampler(m_device, &sampler_desc);
}

inline void wgpu_renderer::create_mesh_buffers(int idx, const mesh::mesh_data& mesh)
{
  m_meshes[idx].vertex_buffer =
    create_wgpu_buffer(static_cast<WGPUBufferUsage>(WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst),
                       mesh.vertices.size() * sizeof(vertex), mesh.vertices.data());
  m_meshes[idx].index_buffer =
    create_wgpu_buffer(static_cast<WGPUBufferUsage>(WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst),
                       mesh.indices.size() * sizeof(uint32_t), mesh.indices.data());
  m_meshes[idx].index_count = static_cast<uint32_t>(mesh.indices.size());
}

inline void wgpu_renderer::update_camera_uniforms(const canvas& c)
{
  camera_uniforms uniforms{};
  float aspect = static_cast<float>(c.m_width) / static_cast<float>(c.m_height);
  float fov_rad = static_cast<float>(c.m_camera.m_fov * 3.14159265 / 180.0);
  uniforms.view = matrix::look_at(c.m_camera.m_pos, c.m_camera.m_center, c.m_camera.m_up);
  uniforms.projection =
    matrix::perspective(fov_rad, aspect, static_cast<float>(c.m_camera.m_near), static_cast<float>(c.m_camera.m_far));
  uniforms.view_projection = matrix::multiply(uniforms.projection, uniforms.view);
  uniforms.camera_pos = to_gpu(c.m_camera.m_pos);
  wgpuQueueWriteBuffer(m_queue, m_camera_buffer, 0, &uniforms, sizeof(uniforms));
}

// ============================================================================
// GLFW Callbacks (static, need C linkage for GLFW)
// ============================================================================

inline void mouse_button_cb(GLFWwindow* window, int button, int action, int mods)
{
  if (button == GLFW_MOUSE_BUTTON_LEFT)
    g_input.mouse.left_down = (action == GLFW_PRESS);
  if (button == GLFW_MOUSE_BUTTON_RIGHT)
    g_input.mouse.right_down = (action == GLFW_PRESS);
  if (button == GLFW_MOUSE_BUTTON_MIDDLE)
    g_input.mouse.middle_down = (action == GLFW_PRESS);

  g_input.shift_held = (mods & GLFW_MOD_SHIFT);
  g_input.ctrl_held = (mods & GLFW_MOD_CONTROL);
}

inline void cursor_pos_cb(GLFWwindow* window, double x, double y)
{
  g_input.mouse.x = x;
  g_input.mouse.y = y;
}

inline void scroll_cb(GLFWwindow* window, double xoffset, double yoffset) { g_input.mouse.scroll_delta += yoffset; }

inline void framebuffer_size_cb(GLFWwindow*, int w, int h)
{
  if (w > 0 && h > 0 && g_wgpu_renderer && g_wgpu_renderer->is_initialized())
  {
    // Trigger resize via begin_frame - it will detect size change
    // Store size for the renderer to pick up
    if (auto* canvas_ptr = static_cast<canvas*>(glfwGetWindowUserPointer(g_wgpu_renderer->window())))
    {
      canvas_ptr->m_width = w;
      canvas_ptr->m_height = h;
      canvas_ptr->mark_dirty();
    }
  }
}

// ============================================================================
// wgpu_renderer - Frame Rendering Methods
// ============================================================================

inline bool wgpu_renderer::begin_frame(canvas& c)
{
  // Process camera input before rendering
  process_camera_input(c);

  // Handle window resize
  int fb_width, fb_height;
  glfwGetFramebufferSize(m_window, &fb_width, &fb_height);
  if (fb_width != m_width || fb_height != m_height)
  {
    if (fb_width > 0 && fb_height > 0)
    {
      m_width = fb_width;
      m_height = fb_height;
      configure_surface();
      create_depth_buffer(fb_width, fb_height);
      c.m_width = fb_width;
      c.m_height = fb_height;
      c.mark_dirty();
    }
  }

  m_current_canvas = &c;

  // Get surface texture
  wgpuSurfaceGetCurrentTexture(m_surface, &m_current_surface_texture);
  if (m_current_surface_texture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
      m_current_surface_texture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal)
    return false;

  m_current_back_buffer = wgpuTextureCreateView(m_current_surface_texture.texture, nullptr);
  if (!m_current_back_buffer)
  {
    wgpuTextureRelease(m_current_surface_texture.texture);
    return false;
  }

  update_camera_uniforms(c);
  return true;
}

inline void wgpu_renderer::end_frame()
{
  if (m_current_back_buffer)
  {
    wgpuTextureViewRelease(m_current_back_buffer);
    m_current_back_buffer = nullptr;
  }

  wgpuSurfacePresent(m_surface);

  if (m_current_surface_texture.texture)
  {
    wgpuTextureRelease(m_current_surface_texture.texture);
    m_current_surface_texture.texture = nullptr;
  }
}

inline void wgpu_renderer::render(canvas& c)
{
  if (!m_current_back_buffer)
    return;

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

  // Collect sphere instances
  std::vector<instance_data> sphere_instances;
  for (const auto& s : c.m_spheres)
  {
    if (!s.m_visible)
      continue;
    sphere_instances.push_back(build_instance(s, vec3{s.m_radius * 2, s.m_radius * 2, s.m_radius * 2}));
  }

  // Collect ellipsoid instances
  std::vector<instance_data> ellipsoid_instances;
  for (const auto& e : c.m_ellipsoids)
  {
    if (!e.m_visible)
      continue;
    ellipsoid_instances.push_back(build_instance(e, vec3{e.m_length, e.m_height, e.m_width}));
  }

  // Collect box instances
  std::vector<instance_data> box_instances;
  for (const auto& b : c.m_boxes)
  {
    if (!b.m_visible)
      continue;
    box_instances.push_back(build_instance(b, vec3{b.m_length, b.m_height, b.m_width}));
  }

  // Collect cylinder instances
  std::vector<instance_data> cylinder_instances;
  for (const auto& obj : c.m_cylinders)
  {
    if (!obj.m_visible)
      continue;
    float len = static_cast<float>(mag(obj.m_axis));
    float dia = static_cast<float>(obj.m_radius * 2.0);
    instance_data inst{};
    gpu_mat4 rot = matrix::align_x_to_axis(obj.m_axis);
    gpu_mat4 sc = matrix::scale(len, dia, dia);
    gpu_mat4 tr = matrix::translate(static_cast<float>(obj.m_pos.x()), static_cast<float>(obj.m_pos.y()),
                                    static_cast<float>(obj.m_pos.z()));
    inst.model = matrix::multiply(tr, matrix::multiply(rot, sc));
    inst.color = to_gpu4(obj.m_color, static_cast<float>(obj.m_opacity));
    float has_tex = obj.m_texture.valid() ? 1.0f : 0.0f;
    inst.material = {static_cast<float>(obj.m_shininess), obj.m_emissive ? 1.0f : 0.0f, 0, has_tex};
    cylinder_instances.push_back(inst);
  }

  // Collect cone instances
  std::vector<instance_data> cone_instances;
  for (const auto& obj : c.m_cones)
  {
    if (!obj.m_visible)
      continue;
    float len = static_cast<float>(mag(obj.m_axis));
    float dia = static_cast<float>(obj.m_radius * 2.0);
    instance_data inst{};
    gpu_mat4 rot = matrix::align_x_to_axis(obj.m_axis);
    gpu_mat4 sc = matrix::scale(len, dia, dia);
    gpu_mat4 tr = matrix::translate(static_cast<float>(obj.m_pos.x()), static_cast<float>(obj.m_pos.y()),
                                    static_cast<float>(obj.m_pos.z()));
    inst.model = matrix::multiply(tr, matrix::multiply(rot, sc));
    inst.color = to_gpu4(obj.m_color, static_cast<float>(obj.m_opacity));
    float has_tex = obj.m_texture.valid() ? 1.0f : 0.0f;
    inst.material = {static_cast<float>(obj.m_shininess), obj.m_emissive ? 1.0f : 0.0f, 0, has_tex};
    cone_instances.push_back(inst);
  }

  // Process arrows (composite: cylinder shaft + cone head)
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
      gpu_mat4 sc = matrix::scale(static_cast<float>(shaft_len), static_cast<float>(shaft_dia),
                                  static_cast<float>(shaft_dia));
      gpu_mat4 tr = matrix::translate(static_cast<float>(obj.m_pos.x()), static_cast<float>(obj.m_pos.y()),
                                      static_cast<float>(obj.m_pos.z()));
      inst.model = matrix::multiply(tr, matrix::multiply(rot, sc));
      inst.color = to_gpu4(obj.m_color, static_cast<float>(obj.m_opacity));
      inst.material = {static_cast<float>(obj.m_shininess), 0, 0, 0};
      cylinder_instances.push_back(inst);
    }

    // Head (Cone)
    {
      instance_data inst{};
      vec3 head_pos = obj.m_pos + hat(obj.m_axis) * shaft_len;
      gpu_mat4 sc = matrix::scale(static_cast<float>(head_len), static_cast<float>(head_dia),
                                  static_cast<float>(head_dia));
      gpu_mat4 tr = matrix::translate(static_cast<float>(head_pos.x()), static_cast<float>(head_pos.y()),
                                      static_cast<float>(head_pos.z()));
      inst.model = matrix::multiply(tr, matrix::multiply(rot, sc));
      inst.color = to_gpu4(obj.m_color, static_cast<float>(obj.m_opacity));
      inst.material = {static_cast<float>(obj.m_shininess), 0, 0, 0};
      cone_instances.push_back(inst);
    }
  }

  // Collect helix instances
  std::vector<instance_data> helix_instances;
  for (const auto& h : c.m_helixes)
  {
    if (!h.m_visible)
      continue;
    double len = mag(h.m_axis);
    if (len < 1e-6)
      continue;
    instance_data inst{};
    gpu_mat4 rot = matrix::align_x_to_axis(h.m_axis);
    gpu_mat4 sc = matrix::scale(static_cast<float>(len), static_cast<float>(h.m_radius), static_cast<float>(h.m_radius));
    gpu_mat4 tr = matrix::translate(static_cast<float>(h.m_pos.x()), static_cast<float>(h.m_pos.y()),
                                    static_cast<float>(h.m_pos.z()));
    inst.model = matrix::multiply(tr, matrix::multiply(rot, sc));
    inst.color = to_gpu4(h.m_color, static_cast<float>(h.m_opacity));
    inst.material = {static_cast<float>(h.m_shininess), 0, 0, 0};
    helix_instances.push_back(inst);
  }

  WGPUCommandEncoderDescriptor enc_desc{};
  WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(m_device, &enc_desc);

  WGPURenderPassColorAttachment color_att{};
  color_att.view = m_current_back_buffer;
  color_att.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
  color_att.loadOp = WGPULoadOp_Clear;
  color_att.storeOp = WGPUStoreOp_Store;
  color_att.clearValue = {c.m_background.x(), c.m_background.y(), c.m_background.z(), 1.0};

  WGPURenderPassDepthStencilAttachment depth_att{};
  depth_att.view = m_depth_view;
  depth_att.depthLoadOp = WGPULoadOp_Clear;
  depth_att.depthStoreOp = WGPUStoreOp_Store;
  depth_att.depthClearValue = 1.0f;

  WGPURenderPassDescriptor pass_desc{};
  pass_desc.colorAttachmentCount = 1;
  pass_desc.colorAttachments = &color_att;
  pass_desc.depthStencilAttachment = &depth_att;

  WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &pass_desc);
  wgpuRenderPassEncoderSetPipeline(pass, m_pipeline);
  wgpuRenderPassEncoderSetBindGroup(pass, 0, m_bind_group, 0, nullptr);

  auto draw_instances = [&](int mesh_idx, const std::vector<instance_data>& instances, WGPUBuffer& buf,
                            std::size_t& cap) {
    if (instances.empty() || !m_meshes[mesh_idx].vertex_buffer)
      return;
    std::size_t inst_size = instances.size() * sizeof(instance_data);
    if (inst_size > cap)
    {
      if (buf)
        wgpuBufferRelease(buf);
      cap = inst_size * 2;
      buf = create_wgpu_buffer(static_cast<WGPUBufferUsage>(WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst), cap);
    }
    wgpuQueueWriteBuffer(m_queue, buf, 0, instances.data(), inst_size);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 0, m_meshes[mesh_idx].vertex_buffer, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 1, buf, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetIndexBuffer(pass, m_meshes[mesh_idx].index_buffer, WGPUIndexFormat_Uint32, 0,
                                        WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderDrawIndexed(pass, m_meshes[mesh_idx].index_count,
                                     static_cast<uint32_t>(instances.size()), 0, 0, 0);
  };

  draw_instances(0, sphere_instances, m_sphere_ib, m_sphere_ib_cap);
  draw_instances(0, ellipsoid_instances, m_ellipsoid_ib, m_ellipsoid_ib_cap);
  draw_instances(1, box_instances, m_box_ib, m_box_ib_cap);
  draw_instances(2, cylinder_instances, m_cylinder_ib, m_cylinder_ib_cap);
  draw_instances(3, cone_instances, m_cone_ib, m_cone_ib_cap);
  draw_instances(4, helix_instances, m_helix_ib, m_helix_ib_cap);

  wgpuRenderPassEncoderEnd(pass);
  wgpuRenderPassEncoderRelease(pass);

  WGPUCommandBufferDescriptor cmd_desc{};
  WGPUCommandBuffer commands = wgpuCommandEncoderFinish(encoder, &cmd_desc);
  wgpuQueueSubmit(m_queue, 1, &commands);
  wgpuCommandBufferRelease(commands);
  wgpuCommandEncoderRelease(encoder);
}

// ============================================================================
// Loop Integration (static callbacks for legacy API)
// ============================================================================

inline bool window_should_close_impl()
{
  return g_wgpu_renderer && g_wgpu_renderer->window() && glfwWindowShouldClose(g_wgpu_renderer->window());
}

inline void poll_events_impl() { glfwPollEvents(); }

inline void render_frame_wrapper(canvas& c)
{
  if (g_wgpu_renderer)
  {
    if (g_wgpu_renderer->begin_frame(c))
    {
      g_wgpu_renderer->render(c);
      g_wgpu_renderer->end_frame();
    }
  }
}

// ============================================================================
// wgpu_renderer - Texture and Sampler Management
// ============================================================================

inline texture_handle wgpu_renderer::create_texture(const texture_desc& desc)
{
  wgpu_texture_resource resource{};
  resource.desc = desc;

  // Convert format
  WGPUTextureFormat wgpu_format = WGPUTextureFormat_RGBA8Unorm;
  switch (desc.format)
  {
  case texture_format::rgba8_unorm: wgpu_format = WGPUTextureFormat_RGBA8Unorm; break;
  case texture_format::bgra8_unorm: wgpu_format = WGPUTextureFormat_BGRA8Unorm; break;
  case texture_format::rgba8_srgb: wgpu_format = WGPUTextureFormat_RGBA8UnormSrgb; break;
  case texture_format::bgra8_srgb: wgpu_format = WGPUTextureFormat_BGRA8UnormSrgb; break;
  case texture_format::rgba16_float: wgpu_format = WGPUTextureFormat_RGBA16Float; break;
  case texture_format::rgba32_float: wgpu_format = WGPUTextureFormat_RGBA32Float; break;
  case texture_format::depth24_plus: wgpu_format = WGPUTextureFormat_Depth24Plus; break;
  case texture_format::depth32_float: wgpu_format = WGPUTextureFormat_Depth32Float; break;
  }

  // Convert usage
  WGPUTextureUsage wgpu_usage = WGPUTextureUsage_None;
  if (has_flag(desc.usage, texture_usage::sampled))
    wgpu_usage = static_cast<WGPUTextureUsage>(wgpu_usage | WGPUTextureUsage_TextureBinding);
  if (has_flag(desc.usage, texture_usage::storage))
    wgpu_usage = static_cast<WGPUTextureUsage>(wgpu_usage | WGPUTextureUsage_StorageBinding);
  if (has_flag(desc.usage, texture_usage::render_target))
    wgpu_usage = static_cast<WGPUTextureUsage>(wgpu_usage | WGPUTextureUsage_RenderAttachment);
  if (has_flag(desc.usage, texture_usage::copy_src))
    wgpu_usage = static_cast<WGPUTextureUsage>(wgpu_usage | WGPUTextureUsage_CopySrc);
  if (has_flag(desc.usage, texture_usage::copy_dst))
    wgpu_usage = static_cast<WGPUTextureUsage>(wgpu_usage | WGPUTextureUsage_CopyDst);

  WGPUTextureDescriptor tex_desc{};
  tex_desc.size = {desc.width, desc.height, desc.depth};
  tex_desc.mipLevelCount = desc.mip_levels;
  tex_desc.sampleCount = 1;
  tex_desc.dimension = WGPUTextureDimension_2D;
  tex_desc.format = wgpu_format;
  tex_desc.usage = wgpu_usage;

  resource.texture = wgpuDeviceCreateTexture(m_device, &tex_desc);
  if (!resource.texture)
    return texture_handle{};

  resource.view = wgpuTextureCreateView(resource.texture, nullptr);

  return m_texture_pool.allocate(std::move(resource));
}

inline void wgpu_renderer::upload_texture(texture_handle h, const image_data& data)
{
  auto* resource = m_texture_pool.get(h);
  if (!resource || !resource->texture)
    return;

  WGPUImageCopyTexture dst{};
  dst.texture = resource->texture;
  dst.mipLevel = 0;
  dst.origin = {0, 0, 0};
  dst.aspect = WGPUTextureAspect_All;

  WGPUTextureDataLayout layout{};
  layout.offset = 0;
  layout.bytesPerRow = data.width * data.channels;
  layout.rowsPerImage = data.height;

  WGPUExtent3D extent = {data.width, data.height, 1};

  wgpuQueueWriteTexture(m_queue, &dst, data.pixels.data(), data.pixels.size(), &layout, &extent);
}

inline void wgpu_renderer::destroy_texture(texture_handle h)
{
  auto* resource = m_texture_pool.get(h);
  if (!resource)
    return;

  if (resource->view)
    wgpuTextureViewRelease(resource->view);
  if (resource->texture)
    wgpuTextureRelease(resource->texture);

  m_texture_pool.release(h);
}

inline sampler_handle wgpu_renderer::create_sampler(const sampler_desc& desc)
{
  wgpu_sampler_resource resource{};
  resource.desc = desc;

  // Convert filter modes
  auto to_wgpu_filter = [](filter_mode f) {
    return f == filter_mode::nearest ? WGPUFilterMode_Nearest : WGPUFilterMode_Linear;
  };
  auto to_wgpu_mipmap_filter = [](filter_mode f) {
    return f == filter_mode::nearest ? WGPUMipmapFilterMode_Nearest : WGPUMipmapFilterMode_Linear;
  };

  // Convert address modes
  auto to_wgpu_address = [](address_mode a) {
    switch (a)
    {
    case address_mode::repeat: return WGPUAddressMode_Repeat;
    case address_mode::mirror_repeat: return WGPUAddressMode_MirrorRepeat;
    case address_mode::clamp_to_edge: return WGPUAddressMode_ClampToEdge;
    case address_mode::clamp_to_border: return WGPUAddressMode_ClampToEdge; // WebGPU doesn't have border
    default: return WGPUAddressMode_Repeat;
    }
  };

  WGPUSamplerDescriptor sampler_desc{};
  sampler_desc.magFilter = to_wgpu_filter(desc.mag_filter);
  sampler_desc.minFilter = to_wgpu_filter(desc.min_filter);
  sampler_desc.mipmapFilter = to_wgpu_mipmap_filter(desc.mipmap_filter);
  sampler_desc.addressModeU = to_wgpu_address(desc.wrap_u);
  sampler_desc.addressModeV = to_wgpu_address(desc.wrap_v);
  sampler_desc.addressModeW = to_wgpu_address(desc.wrap_w);
  sampler_desc.maxAnisotropy = static_cast<uint16_t>(desc.max_anisotropy);

  resource.sampler = wgpuDeviceCreateSampler(m_device, &sampler_desc);
  if (!resource.sampler)
    return sampler_handle{};

  return m_sampler_pool.allocate(std::move(resource));
}

inline void wgpu_renderer::destroy_sampler(sampler_handle h)
{
  auto* resource = m_sampler_pool.get(h);
  if (!resource)
    return;

  if (resource->sampler)
    wgpuSamplerRelease(resource->sampler);

  m_sampler_pool.release(h);
}

// ============================================================================
// wgpu_renderer - Init and Shutdown
// ============================================================================

inline bool wgpu_renderer::init(canvas& c)
{
  if (!glfwInit())
    return false;
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  m_window = glfwCreateWindow(c.m_width, c.m_height, c.m_title.c_str(), nullptr, nullptr);
  if (!m_window)
    return false;

  // Store canvas pointer for resize callback
  glfwSetWindowUserPointer(m_window, &c);

  glfwSetMouseButtonCallback(m_window, mouse_button_cb);
  glfwSetCursorPosCallback(m_window, cursor_pos_cb);
  glfwSetScrollCallback(m_window, scroll_cb);
  glfwSetFramebufferSizeCallback(m_window, framebuffer_size_cb);

  m_width = c.m_width;
  m_height = c.m_height;

  // Create instance
  WGPUInstanceDescriptor inst_desc{};
  m_instance = wgpuCreateInstance(&inst_desc);
  if (!m_instance)
    return false;

  // Create surface
#if defined(__APPLE__)
  {
    void* cocoa_window = glfwGetCocoaWindow(m_window);
    void* metal_layer = vcpp_create_metal_layer(cocoa_window);
    WGPUSurfaceSourceMetalLayer src{};
    src.chain.sType = WGPUSType_SurfaceSourceMetalLayer;
    src.layer = metal_layer;
    WGPUSurfaceDescriptor desc{};
    desc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&src);
    m_surface = wgpuInstanceCreateSurface(m_instance, &desc);
  }
#elif defined(_WIN32)
  {
    WGPUSurfaceSourceWindowsHWND src{};
    src.chain.sType = WGPUSType_SurfaceSourceWindowsHWND;
    src.hwnd = glfwGetWin32Window(m_window);
    src.hinstance = GetModuleHandle(nullptr);
    WGPUSurfaceDescriptor desc{};
    desc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&src);
    m_surface = wgpuInstanceCreateSurface(m_instance, &desc);
  }
#else
  {
    WGPUSurfaceSourceXlibWindow src{};
    src.chain.sType = WGPUSType_SurfaceSourceXlibWindow;
    src.display = glfwGetX11Display();
    src.window = glfwGetX11Window(m_window);
    WGPUSurfaceDescriptor desc{};
    desc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&src);
    m_surface = wgpuInstanceCreateSurface(m_instance, &desc);
  }
#endif

  // Request adapter (blocking)
  struct AdapterData
  {
    WGPUAdapter adapter{nullptr};
    bool done{false};
  } adapter_data;
  WGPURequestAdapterOptions adapter_opts{};
  adapter_opts.compatibleSurface = m_surface;
  WGPURequestAdapterCallbackInfo adapter_cb{};
  adapter_cb.mode = WGPUCallbackMode_AllowSpontaneous;
  adapter_cb.callback = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView, void* ud1, void*) {
    auto* data = static_cast<AdapterData*>(ud1);
    if (status == WGPURequestAdapterStatus_Success)
      data->adapter = adapter;
    data->done = true;
  };
  adapter_cb.userdata1 = &adapter_data;
  wgpuInstanceRequestAdapter(m_instance, &adapter_opts, adapter_cb);
  while (!adapter_data.done)
    wgpuInstanceProcessEvents(m_instance);
  m_adapter = adapter_data.adapter;
  if (!m_adapter)
    return false;

  // Request device (blocking)
  struct DeviceData
  {
    WGPUDevice device{nullptr};
    bool done{false};
  } device_data;
  WGPUDeviceDescriptor device_desc{};
  WGPURequestDeviceCallbackInfo device_cb{};
  device_cb.mode = WGPUCallbackMode_AllowSpontaneous;
  device_cb.callback = [](WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView, void* ud1, void*) {
    auto* data = static_cast<DeviceData*>(ud1);
    if (status == WGPURequestDeviceStatus_Success)
      data->device = device;
    data->done = true;
  };
  device_cb.userdata1 = &device_data;
  wgpuAdapterRequestDevice(m_adapter, &device_desc, device_cb);
  while (!device_data.done)
    wgpuInstanceProcessEvents(m_instance);
  m_device = device_data.device;
  if (!m_device)
    return false;

  m_queue = wgpuDeviceGetQueue(m_device);

  // Configure surface
  configure_surface();
  create_depth_buffer(m_width, m_height);

  // Create default texture and sampler
  create_default_texture();

  // Create shader module
  WGPUShaderSourceWGSL wgsl_src{};
  wgsl_src.chain.sType = WGPUSType_ShaderSourceWGSL;
  wgsl_src.code = {shader_source, WGPU_STRLEN};
  WGPUShaderModuleDescriptor shader_desc{};
  shader_desc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&wgsl_src);
  WGPUShaderModule shader = wgpuDeviceCreateShaderModule(m_device, &shader_desc);

  // Create camera uniform buffer
  m_camera_buffer = create_wgpu_buffer(
    static_cast<WGPUBufferUsage>(WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst), sizeof(camera_uniforms));

  // Create bind group layout with sampler and texture
  WGPUBindGroupLayoutEntry bgl_entries[3] = {};
  // Camera uniform
  bgl_entries[0].binding = 0;
  bgl_entries[0].visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
  bgl_entries[0].buffer.type = WGPUBufferBindingType_Uniform;
  bgl_entries[0].buffer.minBindingSize = sizeof(camera_uniforms);
  // Sampler
  bgl_entries[1].binding = 1;
  bgl_entries[1].visibility = WGPUShaderStage_Fragment;
  bgl_entries[1].sampler.type = WGPUSamplerBindingType_Filtering;
  // Texture
  bgl_entries[2].binding = 2;
  bgl_entries[2].visibility = WGPUShaderStage_Fragment;
  bgl_entries[2].texture.sampleType = WGPUTextureSampleType_Float;
  bgl_entries[2].texture.viewDimension = WGPUTextureViewDimension_2D;
  bgl_entries[2].texture.multisampled = false;

  WGPUBindGroupLayoutDescriptor bgl_desc{};
  bgl_desc.entryCount = 3;
  bgl_desc.entries = bgl_entries;
  m_bind_group_layout = wgpuDeviceCreateBindGroupLayout(m_device, &bgl_desc);

  // Create bind group
  WGPUBindGroupEntry bg_entries[3] = {};
  bg_entries[0].binding = 0;
  bg_entries[0].buffer = m_camera_buffer;
  bg_entries[0].size = sizeof(camera_uniforms);
  bg_entries[1].binding = 1;
  bg_entries[1].sampler = m_default_sampler;
  bg_entries[2].binding = 2;
  bg_entries[2].textureView = m_default_texture_view;

  WGPUBindGroupDescriptor bg_desc{};
  bg_desc.layout = m_bind_group_layout;
  bg_desc.entryCount = 3;
  bg_desc.entries = bg_entries;
  m_bind_group = wgpuDeviceCreateBindGroup(m_device, &bg_desc);

  // Create pipeline layout
  WGPUPipelineLayoutDescriptor pl_desc{};
  pl_desc.bindGroupLayoutCount = 1;
  pl_desc.bindGroupLayouts = &m_bind_group_layout;
  WGPUPipelineLayout pipeline_layout = wgpuDeviceCreatePipelineLayout(m_device, &pl_desc);

  // Vertex attributes for mesh data (position, normal, uv)
  WGPUVertexAttribute mesh_attrs[3] = {};
  mesh_attrs[0].format = WGPUVertexFormat_Float32x3;
  mesh_attrs[0].offset = 0;
  mesh_attrs[0].shaderLocation = 0;
  mesh_attrs[1].format = WGPUVertexFormat_Float32x3;
  mesh_attrs[1].offset = 12;
  mesh_attrs[1].shaderLocation = 1;
  mesh_attrs[2].format = WGPUVertexFormat_Float32x2;
  mesh_attrs[2].offset = 24;
  mesh_attrs[2].shaderLocation = 2;

  // Vertex attributes for instance data (model matrix + color + material)
  WGPUVertexAttribute inst_attrs[6] = {};
  for (int i = 0; i < 4; ++i)
  {
    inst_attrs[i].format = WGPUVertexFormat_Float32x4;
    inst_attrs[i].offset = i * 16;
    inst_attrs[i].shaderLocation = 3 + i;
  }
  inst_attrs[4].format = WGPUVertexFormat_Float32x4;
  inst_attrs[4].offset = 64;
  inst_attrs[4].shaderLocation = 7;
  inst_attrs[5].format = WGPUVertexFormat_Float32x4;
  inst_attrs[5].offset = 80;
  inst_attrs[5].shaderLocation = 8;

  WGPUVertexBufferLayout vb_layouts[2] = {};
  vb_layouts[0].arrayStride = sizeof(vertex);
  vb_layouts[0].stepMode = WGPUVertexStepMode_Vertex;
  vb_layouts[0].attributeCount = 3;
  vb_layouts[0].attributes = mesh_attrs;
  vb_layouts[1].arrayStride = sizeof(instance_data);
  vb_layouts[1].stepMode = WGPUVertexStepMode_Instance;
  vb_layouts[1].attributeCount = 6;
  vb_layouts[1].attributes = inst_attrs;

  WGPUColorTargetState color_target{};
  color_target.format = m_surface_format;
  color_target.writeMask = WGPUColorWriteMask_All;

  WGPUFragmentState frag_state{};
  frag_state.module = shader;
  frag_state.entryPoint = {.data = "fs_main", .length = WGPU_STRLEN};
  frag_state.targetCount = 1;
  frag_state.targets = &color_target;

  WGPUDepthStencilState depth_state{};
  depth_state.format = WGPUTextureFormat_Depth24Plus;
  depth_state.depthWriteEnabled = WGPUOptionalBool_True;
  depth_state.depthCompare = WGPUCompareFunction_Less;

  WGPURenderPipelineDescriptor rp_desc{};
  rp_desc.layout = pipeline_layout;
  rp_desc.vertex.module = shader;
  rp_desc.vertex.entryPoint = {.data = "vs_main", .length = WGPU_STRLEN};
  rp_desc.vertex.bufferCount = 2;
  rp_desc.vertex.buffers = vb_layouts;
  rp_desc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
  rp_desc.primitive.cullMode = WGPUCullMode_None;
  rp_desc.primitive.frontFace = WGPUFrontFace_CCW;
  rp_desc.depthStencil = &depth_state;
  rp_desc.fragment = &frag_state;
  rp_desc.multisample.count = 1;
  rp_desc.multisample.mask = ~0u;

  m_pipeline = wgpuDeviceCreateRenderPipeline(m_device, &rp_desc);

  wgpuShaderModuleRelease(shader);
  wgpuPipelineLayoutRelease(pipeline_layout);

  // Create mesh buffers using shared mesh module
  create_mesh_buffers(0, mesh::generate_sphere());
  create_mesh_buffers(1, mesh::generate_box());
  create_mesh_buffers(2, mesh::generate_cylinder());
  create_mesh_buffers(3, mesh::generate_cone());
  create_mesh_buffers(4, mesh::generate_helix());

  // Hook into vcpp loop (legacy callbacks)
  window_should_close_fn = window_should_close_impl;
  poll_events_fn = poll_events_impl;
  render_frame_fn = render_frame_wrapper;

  // Register with render interface
  set_active_renderer(this);

  m_current_canvas = &c;
  m_initialized = true;

  return true;
}

inline void wgpu_renderer::shutdown()
{
  if (!m_initialized)
    return;

  // Clear resource pools (releases all tracked resources)
  m_texture_pool.for_each([](wgpu_texture_resource& r) {
    if (r.view)
      wgpuTextureViewRelease(r.view);
    if (r.texture)
      wgpuTextureRelease(r.texture);
  });
  m_texture_pool.clear();

  m_sampler_pool.for_each([](wgpu_sampler_resource& r) {
    if (r.sampler)
      wgpuSamplerRelease(r.sampler);
  });
  m_sampler_pool.clear();

  // Release instance buffers
  if (m_sphere_ib)
    wgpuBufferRelease(m_sphere_ib);
  if (m_ellipsoid_ib)
    wgpuBufferRelease(m_ellipsoid_ib);
  if (m_box_ib)
    wgpuBufferRelease(m_box_ib);
  if (m_cylinder_ib)
    wgpuBufferRelease(m_cylinder_ib);
  if (m_cone_ib)
    wgpuBufferRelease(m_cone_ib);
  if (m_helix_ib)
    wgpuBufferRelease(m_helix_ib);
  if (m_camera_buffer)
    wgpuBufferRelease(m_camera_buffer);

  // Release mesh buffers
  for (auto& m : m_meshes)
  {
    if (m.vertex_buffer)
      wgpuBufferRelease(m.vertex_buffer);
    if (m.index_buffer)
      wgpuBufferRelease(m.index_buffer);
  }

  // Release texture resources
  if (m_default_sampler)
    wgpuSamplerRelease(m_default_sampler);
  if (m_default_texture_view)
    wgpuTextureViewRelease(m_default_texture_view);
  if (m_default_texture)
    wgpuTextureRelease(m_default_texture);

  // Release pipeline resources
  if (m_bind_group)
    wgpuBindGroupRelease(m_bind_group);
  if (m_bind_group_layout)
    wgpuBindGroupLayoutRelease(m_bind_group_layout);
  if (m_pipeline)
    wgpuRenderPipelineRelease(m_pipeline);
  if (m_depth_view)
    wgpuTextureViewRelease(m_depth_view);
  if (m_depth_texture)
    wgpuTextureRelease(m_depth_texture);
  if (m_surface)
    wgpuSurfaceRelease(m_surface);
  if (m_queue)
    wgpuQueueRelease(m_queue);
  if (m_device)
    wgpuDeviceRelease(m_device);
  if (m_adapter)
    wgpuAdapterRelease(m_adapter);
  if (m_instance)
    wgpuInstanceRelease(m_instance);

  // Destroy window
  if (m_window)
  {
    glfwDestroyWindow(m_window);
    glfwTerminate();
  }

  // Clear callbacks
  window_should_close_fn = nullptr;
  poll_events_fn = nullptr;
  render_frame_fn = nullptr;

  // Unregister from render interface
  if (get_active_renderer() == this)
    set_active_renderer(nullptr);

  m_initialized = false;
}

// ============================================================================
// Public API (Free Functions - Backward Compatibility)
// ============================================================================

export inline bool init(canvas& c = scene)
{
  if (g_wgpu_renderer)
    return false; // Already initialized

  g_wgpu_renderer = new wgpu_renderer();
  if (!g_wgpu_renderer->init(c))
  {
    delete g_wgpu_renderer;
    g_wgpu_renderer = nullptr;
    return false;
  }
  return true;
}

export inline void shutdown()
{
  if (g_wgpu_renderer)
  {
    g_wgpu_renderer->shutdown();
    delete g_wgpu_renderer;
    g_wgpu_renderer = nullptr;
  }
}

// Access the global renderer instance
export inline wgpu_renderer* get_renderer() { return g_wgpu_renderer; }

} // namespace vcpp::wgpu
