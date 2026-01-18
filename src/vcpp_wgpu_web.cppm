/*
 *  vcpp.wgpu.web - WebGPU rendering backend for browsers (Emscripten)
 *
 *  Uses browser's native WebGPU API via Emscripten bindings.
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

namespace vcpp::wgpu::web {

// ============================================================================
// GPU Types (same as native)
// ============================================================================

struct alignas(16) gpu_vec3 { float x, y, z, _pad; };
struct alignas(16) gpu_vec4 { float x, y, z, w; };
struct alignas(16) gpu_mat4 { float data[16]; };

inline gpu_vec3 to_gpu(const vec3& v) noexcept {
  return {static_cast<float>(v.x()), static_cast<float>(v.y()), static_cast<float>(v.z()), 0.0f};
}

inline gpu_vec4 to_gpu4(const vec3& v, float w = 1.0f) noexcept {
  return {static_cast<float>(v.x()), static_cast<float>(v.y()), static_cast<float>(v.z()), w};
}

// ============================================================================
// Uniforms
// ============================================================================

struct alignas(256) camera_uniforms {
  gpu_mat4 view;
  gpu_mat4 projection;
  gpu_mat4 view_projection;
  gpu_vec3 camera_pos;
};

struct instance_data {
  gpu_mat4 model;
  gpu_vec4 color;
  gpu_vec4 material;
};

// ============================================================================
// Matrix Utilities
// ============================================================================

namespace matrix {

inline gpu_mat4 identity() noexcept {
  gpu_mat4 m{};
  m.data[0] = m.data[5] = m.data[10] = m.data[15] = 1.0f;
  return m;
}

inline gpu_mat4 translate(float x, float y, float z) noexcept {
  gpu_mat4 m = identity();
  m.data[12] = x; m.data[13] = y; m.data[14] = z;
  return m;
}

inline gpu_mat4 scale(float x, float y, float z) noexcept {
  gpu_mat4 m{};
  m.data[0] = x; m.data[5] = y; m.data[10] = z; m.data[15] = 1.0f;
  return m;
}

inline gpu_mat4 multiply(const gpu_mat4& a, const gpu_mat4& b) noexcept {
  gpu_mat4 result{};
  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 4; ++col) {
      float sum = 0.0f;
      for (int k = 0; k < 4; ++k) {
        sum += a.data[row + k * 4] * b.data[k + col * 4];
      }
      result.data[row + col * 4] = sum;
    }
  }
  return result;
}

inline gpu_mat4 look_at(const vec3& eye, const vec3& target, const vec3& up) noexcept {
  vec3 f = hat(target - eye);
  vec3 r = hat(cross(f, up));
  vec3 u = cross(r, f);
  gpu_mat4 m{};
  m.data[0] = static_cast<float>(r.x()); m.data[4] = static_cast<float>(r.y()); m.data[8] = static_cast<float>(r.z());
  m.data[1] = static_cast<float>(u.x()); m.data[5] = static_cast<float>(u.y()); m.data[9] = static_cast<float>(u.z());
  m.data[2] = static_cast<float>(-f.x()); m.data[6] = static_cast<float>(-f.y()); m.data[10] = static_cast<float>(-f.z());
  m.data[12] = static_cast<float>(-dot(r, eye));
  m.data[13] = static_cast<float>(-dot(u, eye));
  m.data[14] = static_cast<float>(dot(f, eye));
  m.data[15] = 1.0f;
  return m;
}

inline gpu_mat4 perspective(float fov_deg, float aspect, float near, float far) noexcept {
  float fov_rad = fov_deg * 3.14159265f / 180.0f;
  float f = 1.0f / std::tan(fov_rad * 0.5f);
  gpu_mat4 m{};
  m.data[0] = f / aspect;
  m.data[5] = f;
  m.data[10] = (far + near) / (near - far);
  m.data[11] = -1.0f;
  m.data[14] = (2.0f * far * near) / (near - far);
  return m;
}

} // namespace matrix

// ============================================================================
// Renderer State
// ============================================================================

struct renderer_state {
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

  WGPUTexture depth_texture{nullptr};
  WGPUTextureView depth_view{nullptr};

  struct mesh_data {
    WGPUBuffer vertex_buffer{nullptr};
    WGPUBuffer index_buffer{nullptr};
    uint32_t index_count{0};
  };
  mesh_data meshes[2];  // 0=sphere, 1=box

  int width{800};
  int height{600};
  canvas* current_canvas{nullptr};
  bool initialized{false};
  bool should_close{false};
};

inline renderer_state g_renderer{};

// ============================================================================
// Mesh Generation (same as native)
// ============================================================================

struct vertex {
  float pos[3];
  float normal[3];
  float uv[2];
};

namespace meshes {

inline std::vector<vertex> generate_sphere(int slices = 16, int stacks = 12) {
  std::vector<vertex> verts;
  for (int i = 0; i <= stacks; ++i) {
    float phi = 3.14159265f * i / stacks;
    float y = std::cos(phi);
    float r = std::sin(phi);
    for (int j = 0; j <= slices; ++j) {
      float theta = 2.0f * 3.14159265f * j / slices;
      float x = r * std::cos(theta);
      float z = r * std::sin(theta);
      vertex v{};
      v.pos[0] = x * 0.5f; v.pos[1] = y * 0.5f; v.pos[2] = z * 0.5f;
      v.normal[0] = x; v.normal[1] = y; v.normal[2] = z;
      v.uv[0] = static_cast<float>(j) / slices;
      v.uv[1] = static_cast<float>(i) / stacks;
      verts.push_back(v);
    }
  }
  return verts;
}

inline std::vector<uint32_t> generate_sphere_indices(int slices = 16, int stacks = 12) {
  std::vector<uint32_t> indices;
  for (int i = 0; i < stacks; ++i) {
    for (int j = 0; j < slices; ++j) {
      uint32_t a = i * (slices + 1) + j;
      uint32_t b = a + slices + 1;
      indices.push_back(a); indices.push_back(b); indices.push_back(a + 1);
      indices.push_back(a + 1); indices.push_back(b); indices.push_back(b + 1);
    }
  }
  return indices;
}

inline std::vector<vertex> generate_box() {
  std::vector<vertex> verts;
  float n = 0.5f;
  auto add_face = [&](float nx, float ny, float nz, float ax, float ay, float az, float bx, float by, float bz) {
    float cx = nx * n, cy = ny * n, cz = nz * n;
    float corners[4][3] = {
      {cx - ax - bx, cy - ay - by, cz - az - bz},
      {cx + ax - bx, cy + ay - by, cz + az - bz},
      {cx + ax + bx, cy + ay + by, cz + az + bz},
      {cx - ax + bx, cy - ay + by, cz - az + bz}
    };
    float uvs[4][2] = {{0,0},{1,0},{1,1},{0,1}};
    for (int i = 0; i < 4; ++i) {
      vertex v{};
      v.pos[0] = corners[i][0]; v.pos[1] = corners[i][1]; v.pos[2] = corners[i][2];
      v.normal[0] = nx; v.normal[1] = ny; v.normal[2] = nz;
      v.uv[0] = uvs[i][0]; v.uv[1] = uvs[i][1];
      verts.push_back(v);
    }
  };
  add_face( 1, 0, 0, 0, n, 0, 0, 0, n);
  add_face(-1, 0, 0, 0, n, 0, 0, 0,-n);
  add_face( 0, 1, 0, n, 0, 0, 0, 0, n);
  add_face( 0,-1, 0, n, 0, 0, 0, 0,-n);
  add_face( 0, 0, 1, n, 0, 0, 0, n, 0);
  add_face( 0, 0,-1,-n, 0, 0, 0, n, 0);
  return verts;
}

inline std::vector<uint32_t> generate_box_indices() {
  std::vector<uint32_t> indices;
  for (uint32_t face = 0; face < 6; ++face) {
    uint32_t base = face * 4;
    indices.push_back(base); indices.push_back(base + 1); indices.push_back(base + 2);
    indices.push_back(base); indices.push_back(base + 2); indices.push_back(base + 3);
  }
  return indices;
}

} // namespace meshes

// ============================================================================
// Shader (WGSL)
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
  @location(3) shininess: f32,
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
  out.shininess = inst.material.x;
  return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
  let light_dir = normalize(vec3<f32>(1.0, 1.0, 1.0));
  let ambient = 0.3;
  let diffuse = max(dot(in.world_normal, light_dir), 0.0) * 0.7;
  let view_dir = normalize(camera.pos - in.world_pos);
  let reflect_dir = reflect(-light_dir, in.world_normal);
  let spec = pow(max(dot(view_dir, reflect_dir), 0.0), 32.0) * in.shininess * 0.5;
  let lighting = ambient + diffuse + spec;
  return vec4<f32>(in.color.rgb * lighting, in.color.a);
}
)";

// ============================================================================
// Helper Functions
// ============================================================================

inline WGPUBuffer create_buffer(WGPUBufferUsage usage, std::size_t size, const void* data = nullptr) {
  WGPUBufferDescriptor desc{};
  desc.size = size;
  desc.usage = static_cast<WGPUBufferUsage>(usage);
  desc.mappedAtCreation = data != nullptr;
  WGPUBuffer buffer = wgpuDeviceCreateBuffer(g_renderer.device, &desc);
  if (data && buffer) {
    void* mapped = wgpuBufferGetMappedRange(buffer, 0, size);
    if (mapped) {
      std::memcpy(mapped, data, size);
      wgpuBufferUnmap(buffer);
    }
  }
  return buffer;
}

// ============================================================================
// Render Frame
// ============================================================================

inline void render_frame() {
  if (!g_renderer.initialized || !g_renderer.current_canvas) return;
  canvas& c = *g_renderer.current_canvas;

  WGPUSurfaceTexture surface_texture{};
  wgpuSurfaceGetCurrentTexture(g_renderer.surface, &surface_texture);
  if (surface_texture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
      surface_texture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal) return;

  WGPUTextureView back_buffer = wgpuTextureCreateView(surface_texture.texture, nullptr);
  if (!back_buffer) return;

  // Update camera uniforms
  camera_uniforms cam{};
  cam.view = matrix::look_at(c.m_camera.m_pos, c.m_camera.m_center, c.m_camera.m_up);
  cam.projection = matrix::perspective(
    static_cast<float>(c.m_camera.m_fov),
    static_cast<float>(g_renderer.width) / g_renderer.height,
    static_cast<float>(c.m_camera.m_near),
    static_cast<float>(c.m_camera.m_far));
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

  // Helper for model matrix (orientation + scale + translation)
  auto compute_model_matrix = [](const object_base& obj, const vec3& scale_factors) {
    vec3 z = hat(obj.m_axis);
    vec3 x = hat(cross(obj.m_up, z));
    vec3 y = cross(z, x);
    gpu_mat4 rot{};
    rot.data[0] = static_cast<float>(x.x()); rot.data[1] = static_cast<float>(x.y()); rot.data[2] = static_cast<float>(x.z());
    rot.data[4] = static_cast<float>(y.x()); rot.data[5] = static_cast<float>(y.y()); rot.data[6] = static_cast<float>(y.z());
    rot.data[8] = static_cast<float>(z.x()); rot.data[9] = static_cast<float>(z.y()); rot.data[10] = static_cast<float>(z.z());
    rot.data[15] = 1.0f;
    gpu_mat4 sc = matrix::scale(static_cast<float>(scale_factors.x()),
      static_cast<float>(scale_factors.y()), static_cast<float>(scale_factors.z()));
    gpu_mat4 tr = matrix::translate(static_cast<float>(obj.m_pos.x()),
      static_cast<float>(obj.m_pos.y()), static_cast<float>(obj.m_pos.z()));
    return matrix::multiply(tr, matrix::multiply(rot, sc));
  };

  // Draw spheres
  std::vector<instance_data> sphere_instances;
  for (const auto& s : c.m_spheres) {
    if (!s.m_visible) continue;
    instance_data inst{};
    gpu_mat4 tr = matrix::translate(
      static_cast<float>(s.m_pos.x()),
      static_cast<float>(s.m_pos.y()),
      static_cast<float>(s.m_pos.z()));
    float scale = static_cast<float>(s.m_radius * 2);
    inst.model = matrix::multiply(tr, matrix::scale(scale, scale, scale));
    inst.color = to_gpu4(s.m_color, static_cast<float>(s.m_opacity));
    inst.material = {static_cast<float>(s.m_shininess), 0, 0, 0};
    sphere_instances.push_back(inst);
  }

  if (!sphere_instances.empty()) {
    std::size_t size = sphere_instances.size() * sizeof(instance_data);
    if (size > g_renderer.sphere_ib_cap) {
      if (g_renderer.sphere_ib) wgpuBufferRelease(g_renderer.sphere_ib);
      g_renderer.sphere_ib_cap = size * 2;
      g_renderer.sphere_ib = create_buffer(
        static_cast<WGPUBufferUsage>(WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst),
        g_renderer.sphere_ib_cap);
    }
    wgpuQueueWriteBuffer(g_renderer.queue, g_renderer.sphere_ib, 0,
                         sphere_instances.data(), size);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 0, g_renderer.meshes[0].vertex_buffer, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 1, g_renderer.sphere_ib, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetIndexBuffer(pass, g_renderer.meshes[0].index_buffer,
                                        WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderDrawIndexed(pass, g_renderer.meshes[0].index_count,
                                     static_cast<uint32_t>(sphere_instances.size()), 0, 0, 0);
  }

  // Draw ellipsoids (reuse sphere mesh, index 0)
  std::vector<instance_data> ellipsoid_instances;
  for (const auto& e : c.m_ellipsoids) {
    if (!e.m_visible) continue;
    instance_data inst{};
    inst.model = compute_model_matrix(e, vec3{e.m_length, e.m_height, e.m_width});
    inst.color = to_gpu4(e.m_color, static_cast<float>(e.m_opacity));
    inst.material = {static_cast<float>(e.m_shininess), e.m_emissive ? 1.0f : 0.0f, 0, 0};
    ellipsoid_instances.push_back(inst);
  }

  if (!ellipsoid_instances.empty()) {
    std::size_t size = ellipsoid_instances.size() * sizeof(instance_data);
    if (size > g_renderer.ellipsoid_ib_cap) {
      if (g_renderer.ellipsoid_ib) wgpuBufferRelease(g_renderer.ellipsoid_ib);
      g_renderer.ellipsoid_ib_cap = size * 2;
      g_renderer.ellipsoid_ib = create_buffer(
        static_cast<WGPUBufferUsage>(WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst),
        g_renderer.ellipsoid_ib_cap);
    }
    wgpuQueueWriteBuffer(g_renderer.queue, g_renderer.ellipsoid_ib, 0,
                         ellipsoid_instances.data(), size);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 0, g_renderer.meshes[0].vertex_buffer, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 1, g_renderer.ellipsoid_ib, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetIndexBuffer(pass, g_renderer.meshes[0].index_buffer,
                                        WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderDrawIndexed(pass, g_renderer.meshes[0].index_count,
                                     static_cast<uint32_t>(ellipsoid_instances.size()), 0, 0, 0);
  }

  // Draw boxes
  std::vector<instance_data> box_instances;
  for (const auto& b : c.m_boxes) {
    if (!b.m_visible) continue;
    instance_data inst{};
    gpu_mat4 tr = matrix::translate(
      static_cast<float>(b.m_pos.x()),
      static_cast<float>(b.m_pos.y()),
      static_cast<float>(b.m_pos.z()));
    inst.model = matrix::multiply(tr, matrix::scale(
      static_cast<float>(b.m_size.x()),
      static_cast<float>(b.m_size.y()),
      static_cast<float>(b.m_size.z())));
    inst.color = to_gpu4(b.m_color, static_cast<float>(b.m_opacity));
    inst.material = {static_cast<float>(b.m_shininess), 0, 0, 0};
    box_instances.push_back(inst);
  }

  if (!box_instances.empty()) {
    std::size_t size = box_instances.size() * sizeof(instance_data);
    if (size > g_renderer.box_ib_cap) {
      if (g_renderer.box_ib) wgpuBufferRelease(g_renderer.box_ib);
      g_renderer.box_ib_cap = size * 2;
      g_renderer.box_ib = create_buffer(
        static_cast<WGPUBufferUsage>(WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst),
        g_renderer.box_ib_cap);
    }
    wgpuQueueWriteBuffer(g_renderer.queue, g_renderer.box_ib, 0,
                         box_instances.data(), size);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 0, g_renderer.meshes[1].vertex_buffer, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 1, g_renderer.box_ib, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetIndexBuffer(pass, g_renderer.meshes[1].index_buffer,
                                        WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderDrawIndexed(pass, g_renderer.meshes[1].index_count,
                                     static_cast<uint32_t>(box_instances.size()), 0, 0, 0);
  }

  wgpuRenderPassEncoderEnd(pass);
  wgpuRenderPassEncoderRelease(pass);

  WGPUCommandBufferDescriptor cmd_desc{};
  WGPUCommandBuffer commands = wgpuCommandEncoderFinish(encoder, &cmd_desc);
  wgpuQueueSubmit(g_renderer.queue, 1, &commands);
  wgpuCommandBufferRelease(commands);
  wgpuCommandEncoderRelease(encoder);
  wgpuTextureViewRelease(back_buffer);
  // Note: wgpuSurfacePresent is not used with emdawnwebgpu - presentation happens automatically
}

// ============================================================================
// Input Callbacks (Emscripten HTML5 API)
// ============================================================================

inline EM_BOOL on_mouse_move(int eventType, const EmscriptenMouseEvent* e, void* userData) {
  (void)eventType; (void)userData;
  vcpp::g_input.mouse.x = e->targetX;
  vcpp::g_input.mouse.y = e->targetY;
  vcpp::g_input.ctrl_held = e->ctrlKey;
  vcpp::g_input.shift_held = e->shiftKey;
  vcpp::g_input.alt_held = e->altKey;
  // Debug: log when mouse moves while button is down
  if (vcpp::g_input.mouse.left_down) {
    emscripten_console_log("[vcpp] mouse move while left down");
  }
  return EM_TRUE;
}

inline EM_BOOL on_mouse_down(int eventType, const EmscriptenMouseEvent* e, void* userData) {
  (void)eventType; (void)userData;
  emscripten_console_log("[vcpp] mouse down!");
  vcpp::g_input.mouse.x = e->targetX;
  vcpp::g_input.mouse.y = e->targetY;
  // Initialize last position on mouse down to prevent jump
  vcpp::g_input.mouse.last_x = e->targetX;
  vcpp::g_input.mouse.last_y = e->targetY;
  vcpp::g_input.ctrl_held = e->ctrlKey;
  vcpp::g_input.shift_held = e->shiftKey;
  vcpp::g_input.alt_held = e->altKey;
  switch (e->button) {
    case 0: vcpp::g_input.mouse.left_down = true; break;
    case 1: vcpp::g_input.mouse.middle_down = true; break;
    case 2: vcpp::g_input.mouse.right_down = true; break;
  }
  return EM_TRUE;
}

inline EM_BOOL on_mouse_up(int eventType, const EmscriptenMouseEvent* e, void* userData) {
  (void)eventType; (void)userData;
  switch (e->button) {
    case 0: vcpp::g_input.mouse.left_down = false; break;
    case 1: vcpp::g_input.mouse.middle_down = false; break;
    case 2: vcpp::g_input.mouse.right_down = false; break;
  }
  return EM_TRUE;
}

inline EM_BOOL on_wheel(int eventType, const EmscriptenWheelEvent* e, void* userData) {
  (void)eventType; (void)userData;
  emscripten_console_log("[vcpp] wheel event!");
  // deltaY > 0 means scroll down (zoom out), < 0 means scroll up (zoom in)
  vcpp::g_input.mouse.scroll_delta += e->deltaY * 0.01;  // Normalize scroll amount
  return EM_TRUE;
}

// Prevent context menu on right-click
inline EM_BOOL on_context_menu(int eventType, const void* reserved, void* userData) {
  (void)eventType; (void)reserved; (void)userData;
  return EM_TRUE;  // Returning true prevents default behavior
}

// ============================================================================
// Main Loop Callback
// ============================================================================

inline void (*user_update_fn)() = nullptr;

inline void main_loop_callback() {
  // Process camera input from mouse state
  if (g_renderer.current_canvas) {
    vcpp::process_camera_input(*g_renderer.current_canvas);
  }
  if (user_update_fn) user_update_fn();
  // Always render every frame for now (animation)
  render_frame();
}

// ============================================================================
// Public API
// ============================================================================

export inline bool init(canvas& c, const char* canvas_selector = "#canvas") {
  // Get WebGPU device from Emscripten
  g_renderer.device = emscripten_webgpu_get_device();
  if (!g_renderer.device) return false;

  g_renderer.queue = wgpuDeviceGetQueue(g_renderer.device);

  // Get canvas size
  double w, h;
  emscripten_get_element_css_size(canvas_selector, &w, &h);
  g_renderer.width = static_cast<int>(w);
  g_renderer.height = static_cast<int>(h);

  // Create surface from canvas
  WGPUEmscriptenSurfaceSourceCanvasHTMLSelector canvas_desc{};
  canvas_desc.chain.sType = WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector;
  canvas_desc.selector = {canvas_selector, WGPU_STRLEN};
  WGPUSurfaceDescriptor surface_desc{};
  surface_desc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&canvas_desc);

  WGPUInstance instance = wgpuCreateInstance(nullptr);
  g_renderer.surface = wgpuInstanceCreateSurface(instance, &surface_desc);

  // Configure surface
  WGPUSurfaceConfiguration config{};
  config.device = g_renderer.device;
  config.format = WGPUTextureFormat_BGRA8Unorm;
  config.usage = WGPUTextureUsage_RenderAttachment;
  config.width = g_renderer.width;
  config.height = g_renderer.height;
  config.presentMode = WGPUPresentMode_Fifo;
  config.alphaMode = WGPUCompositeAlphaMode_Opaque;
  wgpuSurfaceConfigure(g_renderer.surface, &config);

  // Create depth texture
  WGPUTextureDescriptor depth_desc{};
  depth_desc.size = {static_cast<uint32_t>(g_renderer.width),
                     static_cast<uint32_t>(g_renderer.height), 1};
  depth_desc.format = WGPUTextureFormat_Depth24Plus;
  depth_desc.usage = WGPUTextureUsage_RenderAttachment;
  depth_desc.mipLevelCount = 1;
  depth_desc.sampleCount = 1;
  depth_desc.dimension = WGPUTextureDimension_2D;
  g_renderer.depth_texture = wgpuDeviceCreateTexture(g_renderer.device, &depth_desc);
  g_renderer.depth_view = wgpuTextureCreateView(g_renderer.depth_texture, nullptr);

  // Create camera uniform buffer
  g_renderer.camera_buffer = create_buffer(
    static_cast<WGPUBufferUsage>(WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst),
    sizeof(camera_uniforms));

  // Create bind group layout and bind group
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

  // Create shader module
  WGPUShaderSourceWGSL wgsl_desc{};
  wgsl_desc.chain.sType = WGPUSType_ShaderSourceWGSL;
  wgsl_desc.code = {shader_source, WGPU_STRLEN};

  WGPUShaderModuleDescriptor sm_desc{};
  sm_desc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&wgsl_desc);
  WGPUShaderModule shader = wgpuDeviceCreateShaderModule(g_renderer.device, &sm_desc);

  // Create pipeline layout
  WGPUPipelineLayoutDescriptor pl_desc{};
  pl_desc.bindGroupLayoutCount = 1;
  pl_desc.bindGroupLayouts = &g_renderer.bind_group_layout;
  WGPUPipelineLayout pipeline_layout = wgpuDeviceCreatePipelineLayout(g_renderer.device, &pl_desc);

  // Vertex buffer layouts
  WGPUVertexAttribute vertex_attrs[] = {
    {nullptr, WGPUVertexFormat_Float32x3, 0, 0},   // pos
    {nullptr, WGPUVertexFormat_Float32x3, 12, 1},  // normal
    {nullptr, WGPUVertexFormat_Float32x2, 24, 2},  // uv
  };

  WGPUVertexAttribute instance_attrs[] = {
    {nullptr, WGPUVertexFormat_Float32x4, 0, 3},   // model col 0
    {nullptr, WGPUVertexFormat_Float32x4, 16, 4},  // model col 1
    {nullptr, WGPUVertexFormat_Float32x4, 32, 5},  // model col 2
    {nullptr, WGPUVertexFormat_Float32x4, 48, 6},  // model col 3
    {nullptr, WGPUVertexFormat_Float32x4, 64, 7},  // color
    {nullptr, WGPUVertexFormat_Float32x4, 80, 8},  // material
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

  // Create render pipeline
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

  // Create mesh buffers
  auto sphere_verts = meshes::generate_sphere();
  auto sphere_idx = meshes::generate_sphere_indices();
  g_renderer.meshes[0].vertex_buffer = create_buffer(
    static_cast<WGPUBufferUsage>(WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst),
    sphere_verts.size() * sizeof(vertex), sphere_verts.data());
  g_renderer.meshes[0].index_buffer = create_buffer(
    static_cast<WGPUBufferUsage>(WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst),
    sphere_idx.size() * sizeof(uint32_t), sphere_idx.data());
  g_renderer.meshes[0].index_count = static_cast<uint32_t>(sphere_idx.size());

  auto box_verts = meshes::generate_box();
  auto box_idx = meshes::generate_box_indices();
  g_renderer.meshes[1].vertex_buffer = create_buffer(
    static_cast<WGPUBufferUsage>(WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst),
    box_verts.size() * sizeof(vertex), box_verts.data());
  g_renderer.meshes[1].index_buffer = create_buffer(
    static_cast<WGPUBufferUsage>(WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst),
    box_idx.size() * sizeof(uint32_t), box_idx.data());
  g_renderer.meshes[1].index_count = static_cast<uint32_t>(box_idx.size());

  // Register input callbacks on the canvas element
  emscripten_set_mousemove_callback(canvas_selector, nullptr, EM_TRUE, on_mouse_move);
  emscripten_set_mousedown_callback(canvas_selector, nullptr, EM_TRUE, on_mouse_down);
  emscripten_set_mouseup_callback(canvas_selector, nullptr, EM_TRUE, on_mouse_up);
  emscripten_set_wheel_callback(canvas_selector, nullptr, EM_TRUE, on_wheel);
  
  // Prevent context menu on right-click (so right-drag works)
  emscripten_set_click_callback(canvas_selector, nullptr, EM_TRUE,
    [](int, const EmscriptenMouseEvent*, void*) -> EM_BOOL { return EM_TRUE; });

  g_renderer.current_canvas = &c;
  g_renderer.initialized = true;

  return true;
}

export inline void run(void (*update_fn)()) {
  user_update_fn = update_fn;
  emscripten_set_main_loop(main_loop_callback, 0, true);
}

export inline void shutdown() {
  g_renderer.should_close = true;
}

} // namespace vcpp::wgpu::web

#else
// Stub for non-Emscripten builds
namespace vcpp::wgpu::web {
export inline bool init(canvas&, const char* = "#canvas") { return false; }
export inline void run(void (*)()) {}
export inline void shutdown() {}
}
#endif
