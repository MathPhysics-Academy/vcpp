/*
 *  vcpp:render_types - Shared GPU types and matrix utilities
 *
 *  Provides:
 *  - GPU-aligned types: gpu_vec3, gpu_vec4, gpu_mat4
 *  - Uniform structures: camera_uniforms, light_uniforms, instance_data
 *  - Matrix utilities: identity, translate, scale, multiply, look_at, perspective
 *
 *  These are extracted from the backends for code deduplication.
 */

module;

import std;

export module vcpp:render_types;

import :vec;

export namespace vcpp::render
{

// ============================================================================
// GPU-Aligned Types
//
// These match WGSL alignment requirements for uniform buffers.
// ============================================================================

struct alignas(16) gpu_vec3
{
  float x{0}, y{0}, z{0}, _pad{0};

  constexpr gpu_vec3() noexcept = default;
  constexpr gpu_vec3(float x_, float y_, float z_, float pad = 0.0f) noexcept : x{x_}, y{y_}, z{z_}, _pad{pad} {}
};

struct alignas(16) gpu_vec4
{
  float x{0}, y{0}, z{0}, w{1};

  constexpr gpu_vec4() noexcept = default;
  constexpr gpu_vec4(float x_, float y_, float z_, float w_) noexcept : x{x_}, y{y_}, z{z_}, w{w_} {}
};

struct alignas(16) gpu_mat4
{
  float data[16]{};

  constexpr gpu_mat4() noexcept = default;
};

// ============================================================================
// Conversion Helpers
// ============================================================================

inline gpu_vec3 to_gpu(const vec3& v) noexcept
{
  return {static_cast<float>(v.x()), static_cast<float>(v.y()), static_cast<float>(v.z()), 0.0f};
}

inline gpu_vec4 to_gpu4(const vec3& v, float w = 1.0f) noexcept
{
  return {static_cast<float>(v.x()), static_cast<float>(v.y()), static_cast<float>(v.z()), w};
}

// ============================================================================
// Uniform Structures
// ============================================================================

struct alignas(256) camera_uniforms
{
  gpu_mat4 view;
  gpu_mat4 projection;
  gpu_mat4 view_projection;
  gpu_vec3 camera_pos;
};

struct alignas(256) light_uniforms
{
  gpu_vec4 positions[8];
  gpu_vec4 colors[8];
  gpu_vec4 ambient;
  int light_count{0};
  float _pad[3]{};
};

// Instance data for GPU instancing
// material.x = shininess
// material.y = emissive (0 or 1)
// material.z = texture_index
// material.w = has_texture (0 or 1)
struct instance_data
{
  gpu_mat4 model;
  gpu_vec4 color;
  gpu_vec4 material; // x=shininess, y=emissive, z=texture_index, w=has_texture
};

// ============================================================================
// Matrix Utilities
// ============================================================================

namespace matrix
{

inline constexpr gpu_mat4 identity() noexcept
{
  gpu_mat4 m{};
  m.data[0] = m.data[5] = m.data[10] = m.data[15] = 1.0f;
  return m;
}

inline gpu_mat4 translate(float x, float y, float z) noexcept
{
  gpu_mat4 m = identity();
  m.data[12] = x;
  m.data[13] = y;
  m.data[14] = z;
  return m;
}

inline gpu_mat4 scale(float x, float y, float z) noexcept
{
  gpu_mat4 m{};
  m.data[0] = x;
  m.data[5] = y;
  m.data[10] = z;
  m.data[15] = 1.0f;
  return m;
}

inline gpu_mat4 multiply(const gpu_mat4& a, const gpu_mat4& b) noexcept
{
  gpu_mat4 result{};
  for (int row = 0; row < 4; ++row)
  {
    for (int col = 0; col < 4; ++col)
    {
      float sum = 0.0f;
      for (int k = 0; k < 4; ++k)
      {
        sum += a.data[row + k * 4] * b.data[k + col * 4];
      }
      result.data[row + col * 4] = sum;
    }
  }
  return result;
}

inline gpu_mat4 look_at(const vec3& eye, const vec3& center, const vec3& up_vec) noexcept
{
  vec3 f = hat(center - eye);
  vec3 r = hat(cross(f, up_vec));
  vec3 u = cross(r, f);

  gpu_mat4 m = identity();
  m.data[0] = static_cast<float>(r.x());
  m.data[4] = static_cast<float>(r.y());
  m.data[8] = static_cast<float>(r.z());
  m.data[1] = static_cast<float>(u.x());
  m.data[5] = static_cast<float>(u.y());
  m.data[9] = static_cast<float>(u.z());
  m.data[2] = static_cast<float>(-f.x());
  m.data[6] = static_cast<float>(-f.y());
  m.data[10] = static_cast<float>(-f.z());
  m.data[12] = static_cast<float>(-dot(r, eye));
  m.data[13] = static_cast<float>(-dot(u, eye));
  m.data[14] = static_cast<float>(dot(f, eye));
  return m;
}

inline gpu_mat4 perspective(float fov_y_radians, float aspect, float near_plane, float far_plane) noexcept
{
  float f = 1.0f / std::tan(fov_y_radians / 2.0f);
  gpu_mat4 m{};
  m.data[0] = f / aspect;
  m.data[5] = f;
  m.data[10] = (far_plane + near_plane) / (near_plane - far_plane);
  m.data[11] = -1.0f;
  m.data[14] = (2.0f * far_plane * near_plane) / (near_plane - far_plane);
  return m;
}

// Rotation matrix from axis alignment (aligns +X to target axis)
inline gpu_mat4 align_x_to_axis(const vec3& axis) noexcept
{
  vec3 fwd = hat(axis); // New X

  // Handle degenerate cases (zero length)
  if (mag2(axis) < 1e-12)
    return identity();

  // Guide Up vector
  vec3 up_guide{0, 1, 0};

  // If fwd is parallel to up_guide, use Z as guide
  if (std::abs(dot(fwd, up_guide)) > 0.99)
  {
    up_guide = vec3{0, 0, 1};
  }

  vec3 right = hat(cross(fwd, up_guide)); // Z'
  vec3 up_real = cross(right, fwd);       // Y' = Z' cross X'

  gpu_mat4 m{};

  // Column 0: X axis
  m.data[0] = static_cast<float>(fwd.x());
  m.data[1] = static_cast<float>(fwd.y());
  m.data[2] = static_cast<float>(fwd.z());
  m.data[3] = 0.0f;

  // Column 1: Y axis
  m.data[4] = static_cast<float>(up_real.x());
  m.data[5] = static_cast<float>(up_real.y());
  m.data[6] = static_cast<float>(up_real.z());
  m.data[7] = 0.0f;

  // Column 2: Z axis
  m.data[8] = static_cast<float>(right.x());
  m.data[9] = static_cast<float>(right.y());
  m.data[10] = static_cast<float>(right.z());
  m.data[11] = 0.0f;

  // Column 3
  m.data[12] = 0.0f;
  m.data[13] = 0.0f;
  m.data[14] = 0.0f;
  m.data[15] = 1.0f;

  return m;
}

// Rotation matrix from object orientation (axis + up)
inline gpu_mat4 rotation_from_orientation(const vec3& axis, const vec3& up_vec) noexcept
{
  vec3 z = hat(axis);
  vec3 x = hat(cross(up_vec, z));
  vec3 y = cross(z, x);

  gpu_mat4 m{};
  m.data[0] = static_cast<float>(x.x());
  m.data[1] = static_cast<float>(x.y());
  m.data[2] = static_cast<float>(x.z());
  m.data[4] = static_cast<float>(y.x());
  m.data[5] = static_cast<float>(y.y());
  m.data[6] = static_cast<float>(y.z());
  m.data[8] = static_cast<float>(z.x());
  m.data[9] = static_cast<float>(z.y());
  m.data[10] = static_cast<float>(z.z());
  m.data[15] = 1.0f;
  return m;
}

} // namespace matrix

// ============================================================================
// Model Matrix Computation
// ============================================================================

// Compute model matrix from object position, orientation (axis+up), and scale
inline gpu_mat4 compute_model_matrix(const vec3& pos, const vec3& axis, const vec3& up, const vec3& scale_factors) noexcept
{
  gpu_mat4 rot = matrix::rotation_from_orientation(axis, up);
  gpu_mat4 sc = matrix::scale(static_cast<float>(scale_factors.x()), static_cast<float>(scale_factors.y()),
                              static_cast<float>(scale_factors.z()));
  gpu_mat4 tr = matrix::translate(static_cast<float>(pos.x()), static_cast<float>(pos.y()),
                                  static_cast<float>(pos.z()));
  return matrix::multiply(tr, matrix::multiply(rot, sc));
}

} // namespace vcpp::render
