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
import :render_resources;

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
  double m_effect_param0{0.0}; // Custom shader param (packed into material.z when emissive)
  double m_effect_param1{0.0}; // Custom shader param (packed into material.w when emissive)
  bool m_visible{true};

  // Behavior
  bool m_make_trail{false};
  int m_retain{-1};   // trail points kept, newest first (-1 = all)
  int m_interval{-1}; // with a handle's set_pos, a trail point every m_interval moves (-1 = once per render)
  vec3 m_trail_color{1, 1, 1};
  std::string m_trail_type{"curve"}; // or "points": a sphere at each trail point
  double m_trail_radius{0};          // 0: a curve a few pixels wide

  // GlowScript's texture: an image's URL, or ":name" for one of GlowScript's own (see textures::); empty
  // for none. The image multiplies the object's color.
  std::string m_texture;

  // The axis before it was set to zero, restored when it becomes nonzero (GlowScript's __oldaxis)
  std::optional<vec3> m_axis_before_zero;

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

// GlowScript's own textures, named as in its textures table. The images are GlowScript's and aren't part
// of vcpp: the web build copies them from VCPP_TEXTURE_DIR, and ":" names are looked up there.
namespace textures
{
inline constexpr const char* earth = ":earth_texture.jpg";
inline constexpr const char* flower = ":flower_texture.jpg";
inline constexpr const char* granite = ":granite_texture.jpg";
inline constexpr const char* gravel = ":gravel_texture.jpg";
inline constexpr const char* metal = ":metal_texture.jpg";
inline constexpr const char* rock = ":rock_texture.jpg";
inline constexpr const char* rough = ":rough_texture.jpg";
inline constexpr const char* rug = ":rug_texture.jpg";
inline constexpr const char* stones = ":stones_texture.jpg";
inline constexpr const char* stucco = ":stucco_texture.jpg";
inline constexpr const char* wood = ":wood_texture.jpg";
inline constexpr const char* wood_old = ":wood_old_texture.jpg";
} // namespace textures

// ============================================================================
// common_params - Parameter specs for properties in object_base
//
// These are applied to ALL object types by make<T>().
// ============================================================================

inline constexpr auto common_params = std::tuple{param_spec<&object_base::m_pos, decltype(pos)>{},
                                                 param_spec<&object_base::m_axis, decltype(axis)>{},
                                                 param_spec<&object_base::m_up, decltype(up)>{},
                                                 param_spec<&object_base::m_color, decltype(color)>{},
                                                 param_spec<&object_base::m_opacity, decltype(opacity)>{},
                                                 param_spec<&object_base::m_shininess, decltype(shininess)>{},
                                                 param_spec<&object_base::m_emissive, decltype(emissive)>{},
                                                 param_spec<&object_base::m_visible, decltype(visible)>{},
                                                 param_spec<&object_base::m_make_trail, decltype(make_trail)>{},
                                                 param_spec<&object_base::m_retain, decltype(retain)>{},
                                                 param_spec<&object_base::m_interval, decltype(interval)>{},
                                                 param_spec<&object_base::m_trail_color, decltype(trail_color)>{},
                                                 param_spec<&object_base::m_trail_type, decltype(trail_type)>{},
                                                 param_spec<&object_base::m_trail_radius, decltype(trail_radius)>{},
                                                 param_spec<&object_base::m_texture, decltype(texture)>{}};

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
  ObjectType obj{};

  // lam's substitution can't be queried when empty, and box() means all defaults anyway.
  if constexpr (sizeof...(Binders) > 0)
  {
    (check_named_param<Binders, decltype(common_params), decltype(object_params<ObjectType>::value)>(), ...);

    auto params = substitution(binders...);
    using params_t = decltype(params);

    // Apply common parameters to base class
    apply_params(static_cast<object_base&>(obj), params, common_params);

    // Apply object-specific parameters
    apply_params(obj, params, object_params<ObjectType>::value);

    // As in GlowScript, a trail takes the object's colour unless trail_color is given
    if constexpr (!is_bound<decltype(trail_color), params_t>)
      obj.m_trail_color = obj.m_color;

    // GlowScript applies axis before size/length, and links them: an explicit length (or size.x)
    // rescales axis; otherwise axis sets the length.
    if constexpr (length_follows_axis<ObjectType>)
    {
      if constexpr (is_bound<decltype(length), params_t> || is_bound<decltype(size), params_t>)
      {
        const vec3 dir = mag2(obj.m_axis) > 0.0 ? hat(obj.m_axis) : vec3{1, 0, 0};
        obj.m_axis = dir * obj.m_length;
      }
      else if constexpr (is_bound<decltype(axis), params_t>)
        obj.m_length = mag(obj.m_axis);
    }

    // A points trail's spheres default to a tenth of the object's height, as in GlowScript
    if constexpr (!is_bound<decltype(trail_radius), params_t>)
      if (obj.m_trail_type == "points")
      {
        if constexpr (requires { obj.m_height; })
          obj.m_trail_radius = 0.1 * obj.m_height;
        else if constexpr (requires { obj.m_radius; })
          obj.m_trail_radius = 0.2 * obj.m_radius;
      }
  }

  return obj;
}

} // namespace vcpp
