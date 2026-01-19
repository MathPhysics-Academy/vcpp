/*
 *  vcpp:object_base - Base class and factory for all scene objects
 *
 *  Provides:
 *  - object_base: common properties shared by all VPython objects
 *  - common_params: parameter specs for common properties
 *  - make<T>(): generic factory function
 */

module;

import std;

export module vcpp:object_base;

import lam.symbols;
import :vec;
import :color;
import :props;
import :traits;

export namespace vcpp
{

using namespace lam::symbols;

// ============================================================================
// object_base - Common base for all scene objects
//
// Contains properties shared by sphere, box, cylinder, arrow, etc.
// ============================================================================

struct object_base
{
  // Spatial properties
  vec3 m_pos{0, 0, 0};
  vec3 m_axis{1, 0, 0};
  vec3 m_up{0, 1, 0};
  vec3 m_size{1, 1, 1}; // bounding size (interpretation varies by object)

  // Appearance
  vec3 m_color{1, 1, 1}; // white default
  double m_opacity{1.0};
  double m_shininess{0.6};
  bool m_emissive{false};
  bool m_visible{true};

  // Behavior
  bool m_make_trail{false};
  double m_retain{-1.0}; // trail retain time (-1 = infinite)
  vec3 m_trail_color{1, 1, 1};

  // ========== Property Accessors (getter/setter pairs) ==========
  // These enable: ball.pos() and ball.pos(new_value)

  constexpr vec3 get_pos() const noexcept { return m_pos; }
  constexpr void set_pos(const vec3& p) noexcept { m_pos = p; }

  constexpr vec3 get_axis() const noexcept { return m_axis; }
  constexpr void set_axis(const vec3& a) noexcept { m_axis = a; }

  constexpr vec3 get_up() const noexcept { return m_up; }
  constexpr void set_up(const vec3& u) noexcept { m_up = u; }

  constexpr vec3 get_color() const noexcept { return m_color; }
  constexpr void set_color(const vec3& c) noexcept { m_color = c; }

  constexpr double get_opacity() const noexcept { return m_opacity; }
  constexpr void set_opacity(double o) noexcept { m_opacity = o; }

  constexpr bool get_visible() const noexcept { return m_visible; }
  constexpr void set_visible(bool v) noexcept { m_visible = v; }
};

// ============================================================================
// common_params - Parameter specs for properties in object_base
//
// These are applied to ALL object types by make<T>().
// ============================================================================

inline constexpr auto common_params =
  std::tuple{param_spec<&object_base::m_pos, decltype(pos), vec3{0, 0, 0}>{},
             param_spec<&object_base::m_axis, decltype(axis), vec3{1, 0, 0}>{},
             param_spec<&object_base::m_up, decltype(up), vec3{0, 1, 0}>{},
             param_spec<&object_base::m_color, decltype(color), vec3{1, 1, 1}>{},
             param_spec<&object_base::m_opacity, decltype(opacity), 1.0>{},
             param_spec<&object_base::m_shininess, decltype(shininess), 0.6>{},
             param_spec<&object_base::m_emissive, decltype(emissive), false>{},
             param_spec<&object_base::m_visible, decltype(visible), true>{},
             param_spec<&object_base::m_make_trail, decltype(make_trail), false>{},
             param_spec<&object_base::m_retain, decltype(retain), -1.0>{},
             param_spec<&object_base::m_trail_color, decltype(trail_color), vec3{1, 1, 1}>{}};

// ============================================================================
// make<ObjectType> - Generic object factory
//
// Creates an object of type ObjectType, applying:
// 1. Common parameters (from common_params)
// 2. Object-specific parameters (from object_params<ObjectType>::value)
//
// Usage:
//   auto s = make<sphere_object>(pos = vec(0,0,0), radius = 2);
// ============================================================================

template<typename ObjectType, typename... Binders>
constexpr ObjectType make(Binders... binders)
{
  auto params = substitution(binders...);
  ObjectType obj{};

  // Apply common parameters to base class
  apply_params(static_cast<object_base&>(obj), params, common_params);

  // Apply object-specific parameters
  apply_params(obj, params, object_params<ObjectType>::value);

  return obj;
}

} // namespace vcpp
