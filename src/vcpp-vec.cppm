/*
 *  vcpp:vec - 3D vector type and VPython-style operations
 *
 *  Uses lam::linalg::fixed_vector as the underlying type,
 *  providing VPython-compatible API on top.
 */

module;

import std;

export module vcpp:vec;

import lam.linearalgebra;

export namespace vcpp
{

// ============================================================================
// Type Aliases - vec3 is lam's fixed_vector
// ============================================================================

using vec3 = lam::linalg::fixed_vector<double, 3>;
using vec4 = lam::linalg::fixed_vector<double, 4>;
using vec2 = lam::linalg::fixed_vector<double, 2>;

// Float versions for GPU data
using vec3f = lam::linalg::fixed_vector<float, 3>;
using vec4f = lam::linalg::fixed_vector<float, 4>;

// VPython also uses 'vector' as an alias
using vector = vec3;

// ============================================================================
// Factory Function (matches VPython's vec(x,y,z) syntax)
// ============================================================================

constexpr vec3 vec(double x, double y, double z) noexcept { return vec3{x, y, z}; }

constexpr vec2 vec(double x, double y) noexcept { return vec2{x, y}; }

// ============================================================================
// VPython-style Free Functions
// Delegate to lam::linalg algorithms where possible
// ============================================================================

// Magnitude
constexpr double mag(const vec3& v) noexcept { return std::sqrt(v.x() * v.x() + v.y() * v.y() + v.z() * v.z()); }

constexpr double mag2(const vec3& v) noexcept { return v.x() * v.x() + v.y() * v.y() + v.z() * v.z(); }

// Unit vector (normalized)
constexpr vec3 hat(const vec3& v) noexcept
{
  double m = mag(v);
  if (m == 0.0)
    return vec3{0, 0, 0};
  return vec3{v.x() / m, v.y() / m, v.z() / m};
}

constexpr vec3 norm(const vec3& v) noexcept { return hat(v); }

// Dot product
constexpr double dot(const vec3& a, const vec3& b) noexcept { return a.x() * b.x() + a.y() * b.y() + a.z() * b.z(); }

// Cross product
constexpr vec3 cross(const vec3& a, const vec3& b) noexcept
{
  return vec3{a.y() * b.z() - a.z() * b.y(), a.z() * b.x() - a.x() * b.z(), a.x() * b.y() - a.y() * b.x()};
}

// Angle between vectors (radians)
constexpr double diff_angle(const vec3& a, const vec3& b) noexcept
{
  double ma = mag(a);
  double mb = mag(b);
  if (ma == 0.0 || mb == 0.0)
    return 0.0;

  double cos_angle = dot(a, b) / (ma * mb);
  // Clamp for numerical stability
  if (cos_angle > 1.0)
    cos_angle = 1.0;
  if (cos_angle < -1.0)
    cos_angle = -1.0;
  return std::acos(cos_angle);
}

// Projection of a onto b
constexpr vec3 proj(const vec3& a, const vec3& b) noexcept
{
  double b_mag2 = mag2(b);
  if (b_mag2 == 0.0)
    return vec3{0, 0, 0};
  double scalar = dot(a, b) / b_mag2;
  return vec3{b.x() * scalar, b.y() * scalar, b.z() * scalar};
}

// Scalar projection (component of a along b)
constexpr double comp(const vec3& a, const vec3& b) noexcept
{
  double mb = mag(b);
  if (mb == 0.0)
    return 0.0;
  return dot(a, b) / mb;
}

// Rotation around axis (Rodrigues' formula)
constexpr vec3 rotate(const vec3& v, double angle, const vec3& axis = vec3{0, 0, 1}) noexcept
{
  vec3 k = hat(axis);
  double c = std::cos(angle);
  double s = std::sin(angle);
  // v*cos(θ) + (k×v)*sin(θ) + k*(k·v)*(1-cos(θ))
  vec3 k_cross_v = cross(k, v);
  double k_dot_v = dot(k, v);
  return vec3{v.x() * c + k_cross_v.x() * s + k.x() * k_dot_v * (1 - c),
              v.y() * c + k_cross_v.y() * s + k.y() * k_dot_v * (1 - c),
              v.z() * c + k_cross_v.z() * s + k.z() * k_dot_v * (1 - c)};
}

// Random vector with components in [-1, 1]
inline vec3 random_vec()
{
  static std::mt19937 gen{std::random_device{}()};
  static std::uniform_real_distribution<double> dist{-1.0, 1.0};
  return vec3{dist(gen), dist(gen), dist(gen)};
}

// Random unit vector (on unit sphere)
inline vec3 random_unit_vec()
{
  // Rejection sampling for uniform distribution on sphere
  while (true)
  {
    vec3 v = random_vec();
    double m2 = mag2(v);
    if (m2 > 0.0 && m2 <= 1.0)
      return hat(v);
  }
}

// ============================================================================
// Convenience: Component-wise operations
// ============================================================================

constexpr vec3 abs(const vec3& v) noexcept { return vec3{std::abs(v.x()), std::abs(v.y()), std::abs(v.z())}; }

constexpr double max_component(const vec3& v) noexcept { return std::max({v.x(), v.y(), v.z()}); }

constexpr double min_component(const vec3& v) noexcept { return std::min({v.x(), v.y(), v.z()}); }

// ============================================================================
// Constants
// ============================================================================

inline constexpr vec3 vec3_zero{0, 0, 0};
inline constexpr vec3 vec3_x{1, 0, 0};
inline constexpr vec3 vec3_y{0, 1, 0};
inline constexpr vec3 vec3_z{0, 0, 1};

} // namespace vcpp
