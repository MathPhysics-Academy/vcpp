/*
 *  vcpp:render_interface - Abstract renderer interface
 *
 *  Provides:
 *  - Renderer concept for compile-time polymorphism
 *  - renderer_base abstract class for runtime polymorphism
 *  - Texture loading utilities
 *
 *  This enables backend-agnostic rendering code.
 */

module;

import std;

export module vcpp:render_interface;

import :vec;
import :scene;
import :render_types;
import :render_resources;
import :mesh;

export namespace vcpp::render
{

// ============================================================================
// Forward Declarations
// ============================================================================

class renderer_base;

// ============================================================================
// Renderer Concept - Compile-time polymorphism
//
// Any type satisfying this concept can be used as a renderer.
// ============================================================================

template<typename R>
concept Renderer = requires(R r, canvas& c, const texture_desc& td, const sampler_desc& sd,
                            const image_data& img, texture_handle th, const mesh::mesh_data& m) {
  // Lifecycle
  { r.init(c) } -> std::convertible_to<bool>;
  { r.shutdown() } -> std::same_as<void>;

  // Frame rendering
  { r.begin_frame(c) } -> std::convertible_to<bool>;
  { r.end_frame() } -> std::same_as<void>;
  { r.render(c) } -> std::same_as<void>;

  // Texture management
  { r.create_texture(td) } -> std::same_as<texture_handle>;
  { r.upload_texture(th, img) } -> std::same_as<void>;
  { r.destroy_texture(th) } -> std::same_as<void>;

  // Sampler management
  { r.create_sampler(sd) } -> std::same_as<sampler_handle>;
  { r.destroy_sampler(std::declval<sampler_handle>()) } -> std::same_as<void>;
};

// ============================================================================
// renderer_base - Abstract base class for runtime polymorphism
//
// Use this when the renderer type is not known at compile time,
// or when you need to store renderers in containers.
// ============================================================================

class renderer_base
{
public:
  virtual ~renderer_base() = default;

  // Lifecycle
  virtual bool init(canvas& c) = 0;
  virtual void shutdown() = 0;

  // Frame rendering
  virtual bool begin_frame(canvas& c) = 0;
  virtual void end_frame() = 0;
  virtual void render(canvas& c) = 0;

  // Texture management
  virtual texture_handle create_texture(const texture_desc& desc) = 0;
  virtual void upload_texture(texture_handle h, const image_data& data) = 0;
  virtual void destroy_texture(texture_handle h) = 0;

  // Sampler management
  virtual sampler_handle create_sampler(const sampler_desc& desc) = 0;
  virtual void destroy_sampler(sampler_handle h) = 0;

  // Buffer management (optional, may not be exposed in all backends)
  virtual buffer_handle create_buffer(const buffer_desc& desc) { return buffer_handle{}; }
  virtual void upload_buffer(buffer_handle h, const void* data, std::size_t size) {}
  virtual void destroy_buffer(buffer_handle h) {}

  // Utility methods with default implementations
  virtual bool is_initialized() const { return m_initialized; }
  virtual int width() const { return m_width; }
  virtual int height() const { return m_height; }

protected:
  bool m_initialized{false};
  int m_width{800};
  int m_height{600};
};

// ============================================================================
// Texture Loading Utilities
//
// Note: These use stb_image internally if available, otherwise provide
// basic functionality. For full texture loading, the backend implementation
// should provide its own loaders.
// ============================================================================

// Create a solid color texture
inline texture_handle create_solid_texture(renderer_base& r, const vec3& color, std::uint32_t size = 4)
{
  texture_desc desc;
  desc.width = size;
  desc.height = size;
  desc.format = texture_format::rgba8_unorm;
  desc.usage = texture_usage::sampled | texture_usage::copy_dst;

  texture_handle h = r.create_texture(desc);
  if (!h)
    return h;

  image_data img = image_data::solid_color(size, size, static_cast<std::uint8_t>(color.x() * 255),
                                           static_cast<std::uint8_t>(color.y() * 255),
                                           static_cast<std::uint8_t>(color.z() * 255), 255);

  r.upload_texture(h, img);
  return h;
}

// Create a checkerboard texture (useful for debugging UV mapping)
inline texture_handle create_checkerboard_texture(renderer_base& r, std::uint32_t size = 64, std::uint32_t cell_size = 8)
{
  texture_desc desc;
  desc.width = size;
  desc.height = size;
  desc.format = texture_format::rgba8_unorm;
  desc.usage = texture_usage::sampled | texture_usage::copy_dst;

  texture_handle h = r.create_texture(desc);
  if (!h)
    return h;

  image_data img = image_data::checkerboard(size, size, cell_size);
  r.upload_texture(h, img);
  return h;
}

// ============================================================================
// Render Callbacks Type Aliases
//
// These are function pointer types for the loop integration.
// ============================================================================

using window_should_close_fn_t = bool (*)();
using poll_events_fn_t = void (*)();
using render_frame_fn_t = void (*)(canvas&);

// ============================================================================
// Default Renderer State
//
// Provides a place for backends to register themselves.
// ============================================================================

struct renderer_callbacks
{
  window_should_close_fn_t window_should_close{nullptr};
  poll_events_fn_t poll_events{nullptr};
  render_frame_fn_t render_frame{nullptr};
  renderer_base* active_renderer{nullptr};
};

// Global renderer callbacks (set by active backend)
inline renderer_callbacks g_renderer_callbacks{};

// Register a renderer as the active one
inline void set_active_renderer(renderer_base* r)
{
  g_renderer_callbacks.active_renderer = r;
}

inline renderer_base* get_active_renderer()
{
  return g_renderer_callbacks.active_renderer;
}

// ============================================================================
// Shader Source Templates
//
// Common shader snippets that can be used by backends.
// ============================================================================

namespace shader_snippets
{

// Camera uniform struct (WGSL)
inline constexpr const char* camera_struct_wgsl = R"(
struct Camera {
  view: mat4x4<f32>,
  proj: mat4x4<f32>,
  view_proj: mat4x4<f32>,
  pos: vec3<f32>,
}
)";

// Instance data struct (WGSL)
inline constexpr const char* instance_struct_wgsl = R"(
struct InstanceInput {
  @location(3) model_0: vec4<f32>,
  @location(4) model_1: vec4<f32>,
  @location(5) model_2: vec4<f32>,
  @location(6) model_3: vec4<f32>,
  @location(7) color: vec4<f32>,
  @location(8) material: vec4<f32>,
}
)";

// Vertex input with UVs (WGSL)
inline constexpr const char* vertex_input_wgsl = R"(
struct VertexInput {
  @location(0) pos: vec3<f32>,
  @location(1) normal: vec3<f32>,
  @location(2) uv: vec2<f32>,
}
)";

// Vertex output (WGSL)
inline constexpr const char* vertex_output_wgsl = R"(
struct VertexOutput {
  @builtin(position) clip_pos: vec4<f32>,
  @location(0) world_normal: vec3<f32>,
  @location(1) world_pos: vec3<f32>,
  @location(2) color: vec4<f32>,
  @location(3) uv: vec2<f32>,
  @location(4) material: vec4<f32>,
}
)";

// Basic lighting calculation (WGSL function body)
inline constexpr const char* basic_lighting_wgsl = R"(
  let light_dir = normalize(vec3<f32>(1.0, 1.0, 1.0));
  let ambient = 0.3;
  let diffuse = max(dot(in.world_normal, light_dir), 0.0) * 0.7;
  let view_dir = normalize(camera.pos - in.world_pos);
  let reflect_dir = reflect(-light_dir, in.world_normal);
  let shininess = in.material.x;
  let spec = pow(max(dot(view_dir, reflect_dir), 0.0), 32.0) * shininess * 0.5;
  let lighting = ambient + diffuse + spec;
)";

// Texture sampling with fallback (WGSL)
inline constexpr const char* texture_sampling_wgsl = R"(
  var base_color = in.color;
  let has_texture = in.material.w > 0.5;
  if (has_texture) {
    let tex_idx = i32(in.material.z);
    let tex_color = textureSample(textures, tex_sampler, in.uv, tex_idx);
    base_color = base_color * tex_color;
  }
)";

} // namespace shader_snippets

} // namespace vcpp::render
