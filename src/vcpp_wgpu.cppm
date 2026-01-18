/*
 *  vcpp.wgpu - WebGPU rendering backend
 *
 *  Uses wgpu-native with the new WebGPU API (no SwapChain, uses Surface).
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

namespace vcpp::wgpu {

// ============================================================================
// GPU Types
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

struct alignas(256) light_uniforms {
  gpu_vec4 positions[8];
  gpu_vec4 colors[8];
  gpu_vec4 ambient;
  int light_count;
  float _pad[3];
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
      for (int k = 0; k < 4; ++k)
        sum += a.data[row + k * 4] * b.data[k + col * 4];
      result.data[row + col * 4] = sum;
    }
  }
  return result;
}

inline gpu_mat4 look_at(const vec3& eye, const vec3& center, const vec3& up_vec) noexcept {
  vec3 f = hat(center - eye);
  vec3 r = hat(cross(f, up_vec));
  vec3 u = cross(r, f);
  gpu_mat4 m = identity();
  m.data[0] = static_cast<float>(r.x()); m.data[4] = static_cast<float>(r.y()); m.data[8] = static_cast<float>(r.z());
  m.data[1] = static_cast<float>(u.x()); m.data[5] = static_cast<float>(u.y()); m.data[9] = static_cast<float>(u.z());
  m.data[2] = static_cast<float>(-f.x()); m.data[6] = static_cast<float>(-f.y()); m.data[10] = static_cast<float>(-f.z());
  m.data[12] = static_cast<float>(-dot(r, eye));
  m.data[13] = static_cast<float>(-dot(u, eye));
  m.data[14] = static_cast<float>(dot(f, eye));
  return m;
}

inline gpu_mat4 perspective(float fov_y_radians, float aspect, float near_plane, float far_plane) noexcept {
  float f = 1.0f / std::tan(fov_y_radians / 2.0f);
  gpu_mat4 m{};
  m.data[0] = f / aspect;
  m.data[5] = f;
  m.data[10] = (far_plane + near_plane) / (near_plane - far_plane);
  m.data[11] = -1.0f;
  m.data[14] = (2.0f * far_plane * near_plane) / (near_plane - far_plane);
  return m;
}

} // namespace matrix

// ============================================================================
// WGSL Shader
// ============================================================================

inline constexpr const char* shader_source = R"(
struct CameraUniforms {
    view: mat4x4<f32>,
    projection: mat4x4<f32>,
    view_projection: mat4x4<f32>,
    camera_pos: vec3<f32>,
};

@group(0) @binding(0) var<uniform> camera: CameraUniforms;

struct VertexInput {
    @location(0) position: vec3<f32>,
    @location(1) normal: vec3<f32>,
};

struct InstanceInput {
    @location(2) model_0: vec4<f32>,
    @location(3) model_1: vec4<f32>,
    @location(4) model_2: vec4<f32>,
    @location(5) model_3: vec4<f32>,
    @location(6) color: vec4<f32>,
    @location(7) material: vec4<f32>,
};

struct VertexOutput {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) world_normal: vec3<f32>,
    @location(1) color: vec4<f32>,
};

@vertex
fn vs_main(vertex: VertexInput, instance: InstanceInput) -> VertexOutput {
    let model = mat4x4<f32>(instance.model_0, instance.model_1, instance.model_2, instance.model_3);
    let world_pos = model * vec4<f32>(vertex.position, 1.0);
    var out: VertexOutput;
    out.clip_position = camera.view_projection * world_pos;
    out.world_normal = normalize((model * vec4<f32>(vertex.normal, 0.0)).xyz);
    out.color = instance.color;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    let light_dir = normalize(vec3<f32>(1.0, 1.0, 1.0));
    let diffuse = max(dot(in.world_normal, light_dir), 0.0) * 0.7 + 0.3;
    return vec4<f32>(in.color.xyz * diffuse, in.color.w);
}
)";

// ============================================================================
// Mesh Data
// ============================================================================

struct vertex { float position[3]; float normal[3]; };

namespace meshes {

inline std::vector<vertex> generate_sphere(int segments = 16, int rings = 12) {
  std::vector<vertex> vertices;
  for (int ring = 0; ring <= rings; ++ring) {
    float phi = static_cast<float>(ring) / rings * 3.14159265f;
    float y = std::cos(phi), r = std::sin(phi);
    for (int seg = 0; seg <= segments; ++seg) {
      float theta = static_cast<float>(seg) / segments * 2.0f * 3.14159265f;
      float x = r * std::cos(theta), z = r * std::sin(theta);
      vertex v{}; v.position[0] = x; v.position[1] = y; v.position[2] = z;
      v.normal[0] = x; v.normal[1] = y; v.normal[2] = z;
      vertices.push_back(v);
    }
  }
  return vertices;
}

inline std::vector<uint32_t> generate_sphere_indices(int segments = 16, int rings = 12) {
  std::vector<uint32_t> indices;
  for (int ring = 0; ring < rings; ++ring) {
    for (int seg = 0; seg < segments; ++seg) {
      uint32_t curr = ring * (segments + 1) + seg;
      uint32_t next = curr + segments + 1;
      indices.push_back(curr); indices.push_back(next); indices.push_back(curr + 1);
      indices.push_back(curr + 1); indices.push_back(next); indices.push_back(next + 1);
    }
  }
  return indices;
}

inline std::vector<vertex> generate_box() {
  std::vector<vertex> vertices;
  const float n[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
  const float faces[6][4][3] = {
    {{ 0.5f,-0.5f,-0.5f},{ 0.5f, 0.5f,-0.5f},{ 0.5f, 0.5f, 0.5f},{ 0.5f,-0.5f, 0.5f}},
    {{-0.5f,-0.5f, 0.5f},{-0.5f, 0.5f, 0.5f},{-0.5f, 0.5f,-0.5f},{-0.5f,-0.5f,-0.5f}},
    {{-0.5f, 0.5f,-0.5f},{-0.5f, 0.5f, 0.5f},{ 0.5f, 0.5f, 0.5f},{ 0.5f, 0.5f,-0.5f}},
    {{-0.5f,-0.5f, 0.5f},{-0.5f,-0.5f,-0.5f},{ 0.5f,-0.5f,-0.5f},{ 0.5f,-0.5f, 0.5f}},
    {{ 0.5f,-0.5f, 0.5f},{ 0.5f, 0.5f, 0.5f},{-0.5f, 0.5f, 0.5f},{-0.5f,-0.5f, 0.5f}},
    {{-0.5f,-0.5f,-0.5f},{-0.5f, 0.5f,-0.5f},{ 0.5f, 0.5f,-0.5f},{ 0.5f,-0.5f,-0.5f}},
  };
  for (int f = 0; f < 6; ++f) {
    for (int c = 0; c < 4; ++c) {
      vertex v{};
      v.position[0] = faces[f][c][0]; v.position[1] = faces[f][c][1]; v.position[2] = faces[f][c][2];
      v.normal[0] = n[f][0]; v.normal[1] = n[f][1]; v.normal[2] = n[f][2];
      vertices.push_back(v);
    }
  }
  return vertices;
}

inline std::vector<uint32_t> generate_box_indices() {
  std::vector<uint32_t> indices;
  for (uint32_t f = 0; f < 6; ++f) {
    uint32_t b = f * 4;
    indices.push_back(b); indices.push_back(b+1); indices.push_back(b+2);
    indices.push_back(b); indices.push_back(b+2); indices.push_back(b+3);
  }
  return indices;
}

} // namespace meshes

// ============================================================================
// Renderer State
// ============================================================================

struct mesh_buffers {
  WGPUBuffer vertex_buffer{nullptr};
  WGPUBuffer index_buffer{nullptr};
  uint32_t index_count{0};
};

struct renderer_state {
  GLFWwindow* window{nullptr};
  WGPUInstance instance{nullptr};
  WGPUSurface surface{nullptr};
  WGPUAdapter adapter{nullptr};
  WGPUDevice device{nullptr};
  WGPUQueue queue{nullptr};
  WGPUTextureFormat surface_format{WGPUTextureFormat_BGRA8Unorm};
  WGPUTexture depth_texture{nullptr};
  WGPUTextureView depth_view{nullptr};
  WGPURenderPipeline pipeline{nullptr};
  WGPUBindGroupLayout bind_group_layout{nullptr};
  WGPUBindGroup bind_group{nullptr};
  WGPUBuffer camera_buffer{nullptr};
  std::array<mesh_buffers, 8> meshes;
  
  // Instance buffers
  WGPUBuffer sphere_ib{nullptr};
  std::size_t sphere_ib_cap{0};
  WGPUBuffer ellipsoid_ib{nullptr};
  std::size_t ellipsoid_ib_cap{0};
  WGPUBuffer box_ib{nullptr};
  std::size_t box_ib_cap{0};

  int width{800}, height{600};
  bool mouse_down{false};
  double last_mouse_x{0}, last_mouse_y{0};
  canvas* current_canvas{nullptr};
  bool initialized{false};
};

inline renderer_state g_renderer{};

// ============================================================================
// Helpers
// ============================================================================

inline WGPUBuffer create_buffer(WGPUBufferUsage usage, std::size_t size, const void* data = nullptr) {
  WGPUBufferDescriptor desc{};
  desc.usage = usage;
  desc.size = size;
  desc.mappedAtCreation = data != nullptr;
  WGPUBuffer buffer = wgpuDeviceCreateBuffer(g_renderer.device, &desc);
  if (data) {
    void* mapped = wgpuBufferGetMappedRange(buffer, 0, size);
    std::memcpy(mapped, data, size);
    wgpuBufferUnmap(buffer);
  }
  return buffer;
}

inline void create_depth_buffer(int w, int h) {
  if (g_renderer.depth_view) wgpuTextureViewRelease(g_renderer.depth_view);
  if (g_renderer.depth_texture) wgpuTextureRelease(g_renderer.depth_texture);
  WGPUTextureDescriptor desc{};
  desc.size = {static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1};
  desc.mipLevelCount = 1;
  desc.sampleCount = 1;
  desc.dimension = WGPUTextureDimension_2D;
  desc.format = WGPUTextureFormat_Depth24Plus;
  desc.usage = WGPUTextureUsage_RenderAttachment;
  g_renderer.depth_texture = wgpuDeviceCreateTexture(g_renderer.device, &desc);
  WGPUTextureViewDescriptor vdesc{};
  vdesc.format = WGPUTextureFormat_Depth24Plus;
  vdesc.dimension = WGPUTextureViewDimension_2D;
  vdesc.mipLevelCount = 1;
  vdesc.arrayLayerCount = 1;
  g_renderer.depth_view = wgpuTextureCreateView(g_renderer.depth_texture, &vdesc);
}

inline void configure_surface() {
  WGPUSurfaceConfiguration config{};
  config.device = g_renderer.device;
  config.format = g_renderer.surface_format;
  config.usage = WGPUTextureUsage_RenderAttachment;
  config.width = g_renderer.width;
  config.height = g_renderer.height;
  config.presentMode = WGPUPresentMode_Fifo;
  config.alphaMode = WGPUCompositeAlphaMode_Auto;
  wgpuSurfaceConfigure(g_renderer.surface, &config);
}

// ============================================================================
// GLFW Callbacks
// ============================================================================

inline void mouse_button_cb(GLFWwindow* window, int button, int action, int mods) {
  if (button == GLFW_MOUSE_BUTTON_LEFT) g_input.mouse.left_down = (action == GLFW_PRESS);
  if (button == GLFW_MOUSE_BUTTON_RIGHT) g_input.mouse.right_down = (action == GLFW_PRESS);
  if (button == GLFW_MOUSE_BUTTON_MIDDLE) g_input.mouse.middle_down = (action == GLFW_PRESS);
  
  g_input.shift_held = (mods & GLFW_MOD_SHIFT);
  g_input.ctrl_held = (mods & GLFW_MOD_CONTROL);
}

inline void cursor_pos_cb(GLFWwindow* window, double x, double y) {
  g_input.mouse.x = x;
  g_input.mouse.y = y;
}

inline void scroll_cb(GLFWwindow* window, double xoffset, double yoffset) {
  g_input.mouse.scroll_delta += yoffset;
}

inline void framebuffer_size_cb(GLFWwindow*, int w, int h) {
  if (w > 0 && h > 0 && g_renderer.initialized) {
    g_renderer.width = w; g_renderer.height = h;
    configure_surface();
    create_depth_buffer(w, h);
    if (g_renderer.current_canvas) {
      g_renderer.current_canvas->m_width = w;
      g_renderer.current_canvas->m_height = h;
      g_renderer.current_canvas->mark_dirty();
    }
  }
}

// ============================================================================
// Rendering
// ============================================================================

inline void update_camera_uniforms(const canvas& c) {
  camera_uniforms uniforms{};
  float aspect = static_cast<float>(c.m_width) / static_cast<float>(c.m_height);
  float fov_rad = static_cast<float>(c.m_camera.m_fov * 3.14159265 / 180.0);
  uniforms.view = matrix::look_at(c.m_camera.m_pos, c.m_camera.m_center, c.m_camera.m_up);
  uniforms.projection = matrix::perspective(fov_rad, aspect,
    static_cast<float>(c.m_camera.m_near), static_cast<float>(c.m_camera.m_far));
  uniforms.view_projection = matrix::multiply(uniforms.projection, uniforms.view);
  uniforms.camera_pos = to_gpu(c.m_camera.m_pos);
  wgpuQueueWriteBuffer(g_renderer.queue, g_renderer.camera_buffer, 0, &uniforms, sizeof(uniforms));
}

inline gpu_mat4 compute_model_matrix(const object_base& obj, const vec3& scale_factors) {
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
}

inline void render_frame_impl(canvas& c) {
  // Process camera input before rendering
  process_camera_input(c);

  WGPUSurfaceTexture surface_texture{};
  wgpuSurfaceGetCurrentTexture(g_renderer.surface, &surface_texture);
  if (surface_texture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
      surface_texture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal) return;

  WGPUTextureView back_buffer = wgpuTextureCreateView(surface_texture.texture, nullptr);
  if (!back_buffer) { wgpuTextureRelease(surface_texture.texture); return; }

  update_camera_uniforms(c);

  // Collect sphere instances
  std::vector<instance_data> sphere_instances;
  for (const auto& s : c.m_spheres) {
    if (!s.m_visible) continue;
    instance_data inst{};
    inst.model = compute_model_matrix(s, vec3{s.m_radius*2, s.m_radius*2, s.m_radius*2});
    inst.color = to_gpu4(s.m_color, static_cast<float>(s.m_opacity));
    inst.material = {static_cast<float>(s.m_shininess), s.m_emissive ? 1.0f : 0.0f, 0, 0};
    sphere_instances.push_back(inst);
  }

  // Collect ellipsoid instances
  std::vector<instance_data> ellipsoid_instances;
  for (const auto& e : c.m_ellipsoids) {
    if (!e.m_visible) continue;
    instance_data inst{};
    inst.model = compute_model_matrix(e, vec3{e.m_length, e.m_height, e.m_width});
    inst.color = to_gpu4(e.m_color, static_cast<float>(e.m_opacity));
    inst.material = {static_cast<float>(e.m_shininess), e.m_emissive ? 1.0f : 0.0f, 0, 0};
    ellipsoid_instances.push_back(inst);
  }

  // Collect box instances
  std::vector<instance_data> box_instances;
  for (const auto& b : c.m_boxes) {
    if (!b.m_visible) continue;
    instance_data inst{};
    inst.model = compute_model_matrix(b, vec3{b.m_length, b.m_height, b.m_width});
    inst.color = to_gpu4(b.m_color, static_cast<float>(b.m_opacity));
    inst.material = {static_cast<float>(b.m_shininess), b.m_emissive ? 1.0f : 0.0f, 0, 0};
    box_instances.push_back(inst);
  }

  WGPUCommandEncoderDescriptor enc_desc{};
  WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(g_renderer.device, &enc_desc);

  WGPURenderPassColorAttachment color_att{};
  color_att.view = back_buffer;
  color_att.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;  // Required for 2D textures
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
  
  // Set cull mode to None to match web backend fix
  // (This is actually set in init(), but we should verify that later)

  auto draw_instances = [&](int mesh_idx, const std::vector<instance_data>& instances, WGPUBuffer& buf, std::size_t& cap) {
    if (instances.empty() || !g_renderer.meshes[mesh_idx].vertex_buffer) return;
    std::size_t inst_size = instances.size() * sizeof(instance_data);
    if (inst_size > cap) {
      if (buf) wgpuBufferRelease(buf);
      cap = inst_size * 2;
      buf = create_buffer(
        static_cast<WGPUBufferUsage>(WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst),
        cap);
    }
    wgpuQueueWriteBuffer(g_renderer.queue, buf, 0, instances.data(), inst_size);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 0, g_renderer.meshes[mesh_idx].vertex_buffer, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 1, buf, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetIndexBuffer(pass, g_renderer.meshes[mesh_idx].index_buffer, WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderDrawIndexed(pass, g_renderer.meshes[mesh_idx].index_count,
      static_cast<uint32_t>(instances.size()), 0, 0, 0);
  };

  draw_instances(0, sphere_instances, g_renderer.sphere_ib, g_renderer.sphere_ib_cap);
  draw_instances(0, ellipsoid_instances, g_renderer.ellipsoid_ib, g_renderer.ellipsoid_ib_cap);
  draw_instances(1, box_instances, g_renderer.box_ib, g_renderer.box_ib_cap);

  wgpuRenderPassEncoderEnd(pass);
  wgpuRenderPassEncoderRelease(pass);

  WGPUCommandBufferDescriptor cmd_desc{};
  WGPUCommandBuffer commands = wgpuCommandEncoderFinish(encoder, &cmd_desc);
  wgpuQueueSubmit(g_renderer.queue, 1, &commands);
  wgpuCommandBufferRelease(commands);
  wgpuCommandEncoderRelease(encoder);
  wgpuTextureViewRelease(back_buffer);

  wgpuSurfacePresent(g_renderer.surface);
  wgpuTextureRelease(surface_texture.texture);
}

// ============================================================================
// Loop Integration
// ============================================================================

inline bool window_should_close_impl() {
  return g_renderer.window && glfwWindowShouldClose(g_renderer.window);
}

inline void poll_events_impl() { glfwPollEvents(); }

inline void render_frame_wrapper(canvas& c) {
  g_renderer.current_canvas = &c;
  render_frame_impl(c);
}

// ============================================================================
// Public API
// ============================================================================

export inline bool init(canvas& c = scene) {
  if (!glfwInit()) return false;
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  g_renderer.window = glfwCreateWindow(c.m_width, c.m_height, c.m_title.c_str(), nullptr, nullptr);
  if (!g_renderer.window) return false;

  glfwSetMouseButtonCallback(g_renderer.window, mouse_button_cb);
  glfwSetCursorPosCallback(g_renderer.window, cursor_pos_cb);
  glfwSetScrollCallback(g_renderer.window, scroll_cb);
  glfwSetFramebufferSizeCallback(g_renderer.window, framebuffer_size_cb);

  g_renderer.width = c.m_width;
  g_renderer.height = c.m_height;

  // Create instance
  WGPUInstanceDescriptor inst_desc{};
  g_renderer.instance = wgpuCreateInstance(&inst_desc);
  if (!g_renderer.instance) return false;

  // Create surface
#if defined(__APPLE__)
  {
    void* cocoa_window = glfwGetCocoaWindow(g_renderer.window);
    void* metal_layer = vcpp_create_metal_layer(cocoa_window);
    WGPUSurfaceSourceMetalLayer src{};
    src.chain.sType = WGPUSType_SurfaceSourceMetalLayer;
    src.layer = metal_layer;
    WGPUSurfaceDescriptor desc{};
    desc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&src);
    g_renderer.surface = wgpuInstanceCreateSurface(g_renderer.instance, &desc);
  }
#elif defined(_WIN32)
  {
    WGPUSurfaceSourceWindowsHWND src{};
    src.chain.sType = WGPUSType_SurfaceSourceWindowsHWND;
    src.hwnd = glfwGetWin32Window(g_renderer.window);
    src.hinstance = GetModuleHandle(nullptr);
    WGPUSurfaceDescriptor desc{};
    desc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&src);
    g_renderer.surface = wgpuInstanceCreateSurface(g_renderer.instance, &desc);
  }
#else
  {
    WGPUSurfaceSourceXlibWindow src{};
    src.chain.sType = WGPUSType_SurfaceSourceXlibWindow;
    src.display = glfwGetX11Display();
    src.window = glfwGetX11Window(g_renderer.window);
    WGPUSurfaceDescriptor desc{};
    desc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&src);
    g_renderer.surface = wgpuInstanceCreateSurface(g_renderer.instance, &desc);
  }
#endif

  // Request adapter (blocking)
  struct AdapterData { WGPUAdapter adapter{nullptr}; bool done{false}; } adapter_data;
  WGPURequestAdapterOptions adapter_opts{};
  adapter_opts.compatibleSurface = g_renderer.surface;
  WGPURequestAdapterCallbackInfo adapter_cb{};
  adapter_cb.mode = WGPUCallbackMode_AllowSpontaneous;
  adapter_cb.callback = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView, void* ud1, void*) {
    auto* data = static_cast<AdapterData*>(ud1);
    if (status == WGPURequestAdapterStatus_Success) data->adapter = adapter;
    data->done = true;
  };
  adapter_cb.userdata1 = &adapter_data;
  wgpuInstanceRequestAdapter(g_renderer.instance, &adapter_opts, adapter_cb);
  while (!adapter_data.done) wgpuInstanceProcessEvents(g_renderer.instance);
  g_renderer.adapter = adapter_data.adapter;
  if (!g_renderer.adapter) return false;

  // Request device (blocking)
  struct DeviceData { WGPUDevice device{nullptr}; bool done{false}; } device_data;
  WGPUDeviceDescriptor device_desc{};
  WGPURequestDeviceCallbackInfo device_cb{};
  device_cb.mode = WGPUCallbackMode_AllowSpontaneous;
  device_cb.callback = [](WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView, void* ud1, void*) {
    auto* data = static_cast<DeviceData*>(ud1);
    if (status == WGPURequestDeviceStatus_Success) data->device = device;
    data->done = true;
  };
  device_cb.userdata1 = &device_data;
  wgpuAdapterRequestDevice(g_renderer.adapter, &device_desc, device_cb);
  while (!device_data.done) wgpuInstanceProcessEvents(g_renderer.instance);
  g_renderer.device = device_data.device;
  if (!g_renderer.device) return false;

  g_renderer.queue = wgpuDeviceGetQueue(g_renderer.device);

  // Configure surface
  configure_surface();
  create_depth_buffer(g_renderer.width, g_renderer.height);

  // Create shader module
  WGPUShaderSourceWGSL wgsl_src{};
  wgsl_src.chain.sType = WGPUSType_ShaderSourceWGSL;
  wgsl_src.code = {shader_source, WGPU_STRLEN};
  WGPUShaderModuleDescriptor shader_desc{};
  shader_desc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&wgsl_src);
  WGPUShaderModule shader = wgpuDeviceCreateShaderModule(g_renderer.device, &shader_desc);

  // Create camera uniform buffer
  g_renderer.camera_buffer = create_buffer(
    static_cast<WGPUBufferUsage>(WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst),
    sizeof(camera_uniforms));

  // Create bind group layout
  WGPUBindGroupLayoutEntry bgl_entry{};
  bgl_entry.binding = 0;
  bgl_entry.visibility = WGPUShaderStage_Vertex;
  bgl_entry.buffer.type = WGPUBufferBindingType_Uniform;
  bgl_entry.buffer.minBindingSize = sizeof(camera_uniforms);
  WGPUBindGroupLayoutDescriptor bgl_desc{};
  bgl_desc.entryCount = 1;
  bgl_desc.entries = &bgl_entry;
  g_renderer.bind_group_layout = wgpuDeviceCreateBindGroupLayout(g_renderer.device, &bgl_desc);

  // Create bind group
  WGPUBindGroupEntry bg_entry{};
  bg_entry.binding = 0;
  bg_entry.buffer = g_renderer.camera_buffer;
  bg_entry.size = sizeof(camera_uniforms);
  WGPUBindGroupDescriptor bg_desc{};
  bg_desc.layout = g_renderer.bind_group_layout;
  bg_desc.entryCount = 1;
  bg_desc.entries = &bg_entry;
  g_renderer.bind_group = wgpuDeviceCreateBindGroup(g_renderer.device, &bg_desc);

  // Create pipeline layout
  WGPUPipelineLayoutDescriptor pl_desc{};
  pl_desc.bindGroupLayoutCount = 1;
  pl_desc.bindGroupLayouts = &g_renderer.bind_group_layout;
  WGPUPipelineLayout pipeline_layout = wgpuDeviceCreatePipelineLayout(g_renderer.device, &pl_desc);

  // Vertex attributes for mesh data
  WGPUVertexAttribute mesh_attrs[2] = {};
  mesh_attrs[0].format = WGPUVertexFormat_Float32x3;
  mesh_attrs[0].offset = 0;
  mesh_attrs[0].shaderLocation = 0;
  mesh_attrs[1].format = WGPUVertexFormat_Float32x3;
  mesh_attrs[1].offset = 12;
  mesh_attrs[1].shaderLocation = 1;

  // Vertex attributes for instance data (model matrix + color + material)
  WGPUVertexAttribute inst_attrs[6] = {};
  for (int i = 0; i < 4; ++i) {
    inst_attrs[i].format = WGPUVertexFormat_Float32x4;
    inst_attrs[i].offset = i * 16;
    inst_attrs[i].shaderLocation = 2 + i;
  }
  inst_attrs[4].format = WGPUVertexFormat_Float32x4;
  inst_attrs[4].offset = 64;
  inst_attrs[4].shaderLocation = 6;
  inst_attrs[5].format = WGPUVertexFormat_Float32x4;
  inst_attrs[5].offset = 80;
  inst_attrs[5].shaderLocation = 7;

  WGPUVertexBufferLayout vb_layouts[2] = {};
  vb_layouts[0].arrayStride = sizeof(vertex);
  vb_layouts[0].stepMode = WGPUVertexStepMode_Vertex;
  vb_layouts[0].attributeCount = 2;
  vb_layouts[0].attributes = mesh_attrs;
  vb_layouts[1].arrayStride = sizeof(instance_data);
  vb_layouts[1].stepMode = WGPUVertexStepMode_Instance;
  vb_layouts[1].attributeCount = 6;
  vb_layouts[1].attributes = inst_attrs;

  WGPUColorTargetState color_target{};
  color_target.format = g_renderer.surface_format;
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

  // Hook into vcpp loop
  window_should_close_fn = window_should_close_impl;
  poll_events_fn = poll_events_impl;
  render_frame_fn = render_frame_wrapper;

  g_renderer.current_canvas = &c;
  g_renderer.initialized = true;

  return true;
}

export inline void shutdown() {
  if (g_renderer.sphere_ib) wgpuBufferRelease(g_renderer.sphere_ib);
  if (g_renderer.ellipsoid_ib) wgpuBufferRelease(g_renderer.ellipsoid_ib);
  if (g_renderer.box_ib) wgpuBufferRelease(g_renderer.box_ib);
  if (g_renderer.camera_buffer) wgpuBufferRelease(g_renderer.camera_buffer);
  for (auto& m : g_renderer.meshes) {
    if (m.vertex_buffer) wgpuBufferRelease(m.vertex_buffer);
    if (m.index_buffer) wgpuBufferRelease(m.index_buffer);
  }
  if (g_renderer.bind_group) wgpuBindGroupRelease(g_renderer.bind_group);
  if (g_renderer.bind_group_layout) wgpuBindGroupLayoutRelease(g_renderer.bind_group_layout);
  if (g_renderer.pipeline) wgpuRenderPipelineRelease(g_renderer.pipeline);
  if (g_renderer.depth_view) wgpuTextureViewRelease(g_renderer.depth_view);
  if (g_renderer.depth_texture) wgpuTextureRelease(g_renderer.depth_texture);
  if (g_renderer.surface) wgpuSurfaceRelease(g_renderer.surface);
  if (g_renderer.queue) wgpuQueueRelease(g_renderer.queue);
  if (g_renderer.device) wgpuDeviceRelease(g_renderer.device);
  if (g_renderer.adapter) wgpuAdapterRelease(g_renderer.adapter);
  if (g_renderer.instance) wgpuInstanceRelease(g_renderer.instance);
  if (g_renderer.window) { glfwDestroyWindow(g_renderer.window); glfwTerminate(); }
  window_should_close_fn = nullptr;
  poll_events_fn = nullptr;
  render_frame_fn = nullptr;
}

} // namespace vcpp::wgpu
