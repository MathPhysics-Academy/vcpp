/*
 *  vcpp:objects - VPython 3D object types
 *
 *  Defines: sphere, box, cylinder, cone, arrow, ring, helix
 *  Each object derives from object_base and adds specific properties.
 */

module;

import std;

export module vcpp:objects;

import lam.symbols;
import :vec;
import :props;
import :traits;
import :object_base;
import :mesh;

export namespace vcpp
{

using namespace lam::symbols;

// ============================================================================
// size= setters
//
// GlowScript stores every object's dimensions as one vector, `size`; length,
// height, width and radius are views onto it. vcpp keeps per-type members, so
// size= fans out to them here. A size those members can't hold exactly throws
// rather than being cut down to fit.
// ============================================================================

namespace detail
{
constexpr bool same_extent(double a, double b) noexcept
{ return a == b || std::abs(a - b) <= 1e-9 * std::max(std::abs(a), std::abs(b)); }

// box, ellipsoid, pyramid: size = (length, height, width)
template<typename Object>
constexpr void set_size_lhw(Object& o, const vec3& s)
{
  o.m_length = s.x();
  o.m_height = s.y();
  o.m_width = s.z();
}

// cylinder, cone, helix: size = (length, 2*radius, 2*radius); one radius, so size.y must equal size.z
template<typename Object>
constexpr void set_size_lr(Object& o, const vec3& s)
{
  if (!same_extent(s.y(), s.z()))
    throw std::invalid_argument("vcpp: size.y and size.z must match; this object has a single radius");
  o.m_length = s.x();
  o.m_radius = s.y() / 2;
}
} // namespace detail

// ============================================================================
// SPHERE
// ============================================================================

struct sphere_object : object_base
{
  double m_radius{1.0};

  // Accessors
  constexpr double get_radius() const noexcept { return m_radius; }
  constexpr void set_radius(double r) noexcept { m_radius = r; }
};

namespace detail
{
// sphere: size = (2*radius, 2*radius, 2*radius); one radius, so all three must match
constexpr void set_size_sphere(sphere_object& o, const vec3& s)
{
  if (!same_extent(s.x(), s.y()) || !same_extent(s.x(), s.z()))
    throw std::invalid_argument("vcpp: a sphere's size must be uniform; it has a single radius");
  o.m_radius = s.x() / 2;
}
} // namespace detail

template<>
struct object_params<sphere_object>
{
  static constexpr auto value = std::tuple{param_spec<&sphere_object::m_radius, decltype(radius)>{},
                                           param_spec<&detail::set_size_sphere, decltype(size)>{}};
};

// build::sphere(...) and the other build:: factories make an object without adding it to a canvas;
// vcpp::sphere(...) (vcpp:scene) makes one in the selected canvas, as VPython does
namespace build
{
template<typename... Binders>
constexpr sphere_object sphere(Binders... binders)
{
  return make<sphere_object>(binders...);
}
} // namespace build

// ============================================================================
// ELLIPSOID
// ============================================================================

struct ellipsoid_object : object_base
{
  double m_length{1.0};
  double m_height{1.0};
  double m_width{1.0};

  // Accessors
  constexpr double get_length() const noexcept { return m_length; }
  constexpr void set_length(double l) noexcept { m_length = l; }

  constexpr double get_height() const noexcept { return m_height; }
  constexpr void set_height(double h) noexcept { m_height = h; }

  constexpr double get_width() const noexcept { return m_width; }
  constexpr void set_width(double w) noexcept { m_width = w; }
};

template<>
struct object_params<ellipsoid_object>
{
  static constexpr auto value = std::tuple{param_spec<&ellipsoid_object::m_length, decltype(length)>{},
                                           param_spec<&ellipsoid_object::m_height, decltype(height)>{},
                                           param_spec<&ellipsoid_object::m_width, decltype(width)>{},
                                           param_spec<&detail::set_size_lhw<ellipsoid_object>, decltype(size)>{}};
};

template<>
inline constexpr bool length_follows_axis<ellipsoid_object> = true;

namespace build
{
template<typename... Binders>
constexpr ellipsoid_object ellipsoid(Binders... binders)
{
  return make<ellipsoid_object>(binders...);
}
} // namespace build

// ============================================================================
// BOX
// ============================================================================

struct box_object : object_base
{
  double m_length{1.0};
  double m_height{1.0};
  double m_width{1.0};

  // Accessors
  constexpr double get_length() const noexcept { return m_length; }
  constexpr void set_length(double l) noexcept { m_length = l; }

  constexpr double get_height() const noexcept { return m_height; }
  constexpr void set_height(double h) noexcept { m_height = h; }

  constexpr double get_width() const noexcept { return m_width; }
  constexpr void set_width(double w) noexcept { m_width = w; }
};

template<>
struct object_params<box_object>
{
  static constexpr auto value = std::tuple{param_spec<&box_object::m_length, decltype(length)>{},
                                           param_spec<&box_object::m_height, decltype(height)>{},
                                           param_spec<&box_object::m_width, decltype(width)>{},
                                           param_spec<&detail::set_size_lhw<box_object>, decltype(size)>{}};
};

template<>
inline constexpr bool length_follows_axis<box_object> = true;

namespace build
{
template<typename... Binders>
constexpr box_object box(Binders... binders)
{
  return make<box_object>(binders...);
}
} // namespace build

// ============================================================================
// CYLINDER
// ============================================================================

struct cylinder_object : object_base
{
  double m_radius{1.0};
  double m_length{1.0};

  // Accessors
  constexpr double get_radius() const noexcept { return m_radius; }
  constexpr void set_radius(double r) noexcept { m_radius = r; }

  constexpr double get_length() const noexcept { return m_length; }
  constexpr void set_length(double l) noexcept { m_length = l; }
};

template<>
struct object_params<cylinder_object>
{
  static constexpr auto value = std::tuple{param_spec<&cylinder_object::m_radius, decltype(radius)>{},
                                           param_spec<&cylinder_object::m_length, decltype(length)>{},
                                           param_spec<&detail::set_size_lr<cylinder_object>, decltype(size)>{}};
};

template<>
inline constexpr bool length_follows_axis<cylinder_object> = true;

namespace build
{
template<typename... Binders>
constexpr cylinder_object cylinder(Binders... binders)
{
  return make<cylinder_object>(binders...);
}
} // namespace build

// ============================================================================
// CONE
// ============================================================================

struct cone_object : object_base
{
  double m_radius{1.0};
  double m_length{1.0};

  // Accessors
  constexpr double get_radius() const noexcept { return m_radius; }
  constexpr void set_radius(double r) noexcept { m_radius = r; }

  constexpr double get_length() const noexcept { return m_length; }
  constexpr void set_length(double l) noexcept { m_length = l; }
};

template<>
struct object_params<cone_object>
{
  static constexpr auto value = std::tuple{param_spec<&cone_object::m_radius, decltype(radius)>{},
                                           param_spec<&cone_object::m_length, decltype(length)>{},
                                           param_spec<&detail::set_size_lr<cone_object>, decltype(size)>{}};
};

template<>
inline constexpr bool length_follows_axis<cone_object> = true;

namespace build
{
template<typename... Binders>
constexpr cone_object cone(Binders... binders)
{
  return make<cone_object>(binders...);
}
} // namespace build

// ============================================================================
// ARROW
// ============================================================================

struct arrow_object : object_base
{
  double m_shaftwidth{0.1};
  double m_headwidth{0.2};
  double m_headlength{0.3};
  bool m_round{false};

  // Accessors
  constexpr double get_shaftwidth() const noexcept { return m_shaftwidth; }
  constexpr void set_shaftwidth(double w) noexcept { m_shaftwidth = w; }

  constexpr double get_headwidth() const noexcept { return m_headwidth; }
  constexpr void set_headwidth(double w) noexcept { m_headwidth = w; }

  constexpr double get_headlength() const noexcept { return m_headlength; }
  constexpr void set_headlength(double l) noexcept { m_headlength = l; }

  constexpr bool get_round() const noexcept { return m_round; }
  constexpr void set_round(bool r) noexcept { m_round = r; }
};

template<>
struct object_params<arrow_object>
{
  static constexpr auto value = std::tuple{param_spec<&arrow_object::m_shaftwidth, decltype(shaftwidth)>{},
                                           param_spec<&arrow_object::m_headwidth, decltype(headwidth)>{},
                                           param_spec<&arrow_object::m_headlength, decltype(headlength)>{},
                                           param_spec<&arrow_object::m_round, decltype(round)>{}};
};

namespace build
{
template<typename... Binders>
constexpr arrow_object arrow(Binders... binders)
{
  return make<arrow_object>(binders...);
}
} // namespace build

// ============================================================================
// RING
// ============================================================================

struct ring_object : object_base
{
  double m_radius{1.0};
  double m_thickness{0.1}; // cross-section radius

  // Accessors
  constexpr double get_radius() const noexcept { return m_radius; }
  constexpr void set_radius(double r) noexcept { m_radius = r; }

  constexpr double get_thickness() const noexcept { return m_thickness; }
  constexpr void set_thickness(double t) noexcept { m_thickness = t; }
};

namespace detail
{
// As in GlowScript, a ring given only a radius is 0.1 x radius thick; a thickness given too is applied after
constexpr void set_ring_radius(ring_object& r, double radius)
{
  r.m_radius = radius;
  r.m_thickness = 0.1 * radius;
}
} // namespace detail

template<>
struct object_params<ring_object>
{
  static constexpr auto value = std::tuple{param_spec<&detail::set_ring_radius, decltype(radius)>{},
                                           param_spec<&ring_object::m_thickness, decltype(thickness)>{}};
};

namespace build
{
template<typename... Binders>
constexpr ring_object ring(Binders... binders)
{
  return make<ring_object>(binders...);
}
} // namespace build

// ============================================================================
// HELIX
// ============================================================================

struct helix_object : object_base
{
  double m_radius{1.0};
  double m_thickness{0.0}; // wire diameter; 0 means radius / 10, as in GlowScript
  double m_length{1.0};
  int m_coils{5};
  bool m_ccw{true}; // counter-clockwise

  // Accessors
  constexpr double get_radius() const noexcept { return m_radius; }
  constexpr void set_radius(double r) noexcept { m_radius = r; }

  constexpr double get_thickness() const noexcept { return m_thickness; }
  constexpr void set_thickness(double t) noexcept { m_thickness = t; }

  constexpr double get_length() const noexcept { return m_length; }
  constexpr void set_length(double l) noexcept { m_length = l; }

  constexpr int get_coils() const noexcept { return m_coils; }
  constexpr void set_coils(int c) noexcept { m_coils = c; }

  constexpr bool get_ccw() const noexcept { return m_ccw; }
  constexpr void set_ccw(bool c) noexcept { m_ccw = c; }
};

template<>
struct object_params<helix_object>
{
  static constexpr auto value = std::tuple{param_spec<&helix_object::m_radius, decltype(radius)>{},
                                           param_spec<&helix_object::m_thickness, decltype(thickness)>{},
                                           param_spec<&helix_object::m_length, decltype(length)>{},
                                           param_spec<&helix_object::m_coils, decltype(coils)>{},
                                           param_spec<&helix_object::m_ccw, decltype(ccw)>{},
                                           param_spec<&detail::set_size_lr<helix_object>, decltype(size)>{}};
};

template<>
inline constexpr bool length_follows_axis<helix_object> = true;

namespace build
{
template<typename... Binders>
constexpr helix_object helix(Binders... binders)
{
  return make<helix_object>(binders...);
}
} // namespace build

// ============================================================================
// PYRAMID (bonus - not in standard VPython but useful)
// ============================================================================

struct pyramid_object : object_base
{
  double m_length{1.0}; // base to apex
  double m_height{1.0}; // base dimension
  double m_width{1.0};  // base dimension
};

template<>
struct object_params<pyramid_object>
{
  static constexpr auto value = std::tuple{param_spec<&pyramid_object::m_length, decltype(length)>{},
                                           param_spec<&pyramid_object::m_height, decltype(height)>{},
                                           param_spec<&pyramid_object::m_width, decltype(width)>{},
                                           param_spec<&detail::set_size_lhw<pyramid_object>, decltype(size)>{}};
};

template<>
inline constexpr bool length_follows_axis<pyramid_object> = true;

namespace build
{
template<typename... Binders>
constexpr pyramid_object pyramid(Binders... binders)
{
  return make<pyramid_object>(binders...);
}
} // namespace build

// ============================================================================
// CURVE (VPython-compatible curve object)
// ============================================================================

// A point of a curve, as GlowScript keeps it: a color of (-1, -1, -1) or a radius of 0 means the curve's own.
// curve::point(n) returns one, so the fields have VPython's names.
struct curve_point
{
  vec3 pos{};
  vec3 color{-1.0, -1.0, -1.0};
  double radius{0};
};

// The named parameters a curve point takes: append(pos = ..., color = ..., radius = ...), modify(n, ...)
inline constexpr auto curve_point_params =
  std::tuple{param_spec<&curve_point::pos, decltype(pos)>{}, param_spec<&curve_point::color, decltype(color)>{},
             param_spec<&curve_point::radius, decltype(radius)>{}};

struct curve_object : object_base
{
  std::vector<curve_point> m_points;
  double m_radius{0}; // 0: a thin line a few pixels wide, as in GlowScript
  mutable bool m_geometry_dirty{true};

  // GlowScript's curve methods
  void append(const curve_point& p)
  {
    m_points.push_back(p);
    m_geometry_dirty = true;
  }
  void append(const vec3& p) { append(curve_point{.pos = p}); }
  void append(const std::vector<vec3>& ps)
  {
    for (const vec3& p : ps)
      append(p);
  }
  // append(pos = p, color = c, radius = r): the color and radius are this point's own
  template<typename... Binders>
    requires(sizeof...(Binders) > 0)
  void append(Binders... binders)
  {
    (check_named_param<Binders, decltype(curve_point_params)>(), ...);
    const auto params = substitution(binders...);
    static_assert(is_bound<decltype(pos), decltype(params)>, "vcpp: a curve point needs a pos");
    curve_point p{};
    apply_params(p, params, curve_point_params);
    append(p);
  }

  // A copy of point n, counting from the end if negative
  curve_point point(std::ptrdiff_t n) const { return m_points[index(n)]; }

  void modify(std::ptrdiff_t n, const vec3& p)
  {
    m_points[index(n)].pos = p;
    m_geometry_dirty = true;
  }
  template<typename... Binders>
    requires(sizeof...(Binders) > 0)
  void modify(std::ptrdiff_t n, Binders... binders)
  {
    (check_named_param<Binders, decltype(curve_point_params)>(), ...);
    apply_params(m_points[index(n)], substitution(binders...), curve_point_params);
    m_geometry_dirty = true;
  }

  void clear()
  {
    m_points.clear();
    m_geometry_dirty = true;
  }

  std::size_t npoints() const { return m_points.size(); }

  // Accessors
  double get_radius() const noexcept { return m_radius; }
  void set_radius(double r) noexcept
  {
    m_radius = r;
    m_geometry_dirty = true;
  }

private:
  std::size_t index(std::ptrdiff_t n) const
  {
    const auto size = static_cast<std::ptrdiff_t>(m_points.size());
    const std::ptrdiff_t i = n < 0 ? n + size : n;
    if (i < 0 || i >= size)
      throw std::out_of_range(std::format("vcpp: curve point {} doesn't exist; the curve has {} points", n, size));
    return static_cast<std::size_t>(i);
  }
};

template<>
struct object_params<curve_object>
{
  static constexpr auto value = std::tuple{param_spec<&curve_object::m_radius, decltype(radius)>{}};
};

namespace build
{
template<typename... Binders>
curve_object curve(Binders... binders)
{
  return make<curve_object>(binders...);
}
} // namespace build

// ============================================================================
// POINTS (VPython-compatible points cloud object)
// ============================================================================

struct points_object : object_base
{
  std::vector<vec3> m_points;    // Point positions
  double m_size{5.0};            // Point size in pixels
  mutable bool m_geometry_dirty{true};

  // VPython-compatible API
  void append(const vec3& p)
  {
    m_points.push_back(p);
    m_geometry_dirty = true;
  }

  void append(const std::vector<vec3>& pts)
  {
    m_points.insert(m_points.end(), pts.begin(), pts.end());
    m_geometry_dirty = true;
  }

  void clear_points()
  {
    m_points.clear();
    m_geometry_dirty = true;
  }

  std::size_t npoints() const { return m_points.size(); }

  // Accessors
  double get_size() const noexcept { return m_size; }
  void set_size(double s) noexcept
  {
    m_size = s;
    m_geometry_dirty = true;
  }
};

template<>
struct object_params<points_object>
{
  static constexpr auto value = std::tuple{param_spec<&points_object::m_size, decltype(size)>{}};
};

namespace build
{
template<typename... Binders>
points_object points(Binders... binders)
{
  return make<points_object>(binders...);
}
} // namespace build

// ============================================================================
// LABEL (2D text overlay at 3D position)
// ============================================================================

struct label_object : object_base
{
  std::string m_text;
  double m_height{15.0};         // Font height in pixels
  std::string m_font{"sans-serif"};
  bool m_billboard{true};        // Always face camera
  double m_xoffset{0.0};         // Screen offset X
  double m_yoffset{0.0};         // Screen offset Y
  bool m_box{false};             // Draw background box
  bool m_line{true};             // Draw line from pos to label
  double m_border{5.0};          // Border padding
  vec3 m_background{0, 0, 0};    // Background color
  double m_background_opacity{0.66};

  // Setters for convenience
  label_object& text(const std::string& t)
  {
    m_text = t;
    return *this;
  }
  label_object& font(const std::string& f)
  {
    m_font = f;
    return *this;
  }
};

template<>
struct object_params<label_object>
{
  static constexpr auto value = std::tuple{param_spec<&label_object::m_text, decltype(text)>{},
                                           param_spec<&label_object::m_height, decltype(height)>{},
                                           param_spec<&label_object::m_billboard, decltype(billboard)>{},
                                           param_spec<&label_object::m_xoffset, decltype(xoffset)>{},
                                           param_spec<&label_object::m_yoffset, decltype(yoffset)>{},
                                           param_spec<&label_object::m_box, decltype(prop::box)>{}};
};

namespace build
{
template<typename... Binders>
label_object label(Binders... binders)
{
  return make<label_object>(binders...);
}
} // namespace build

// ============================================================================
// VERTEX DATA (for triangles/quads)
// ============================================================================

struct vertex_data
{
  vec3 pos{0, 0, 0};
  vec3 normal{0, 1, 0};
  vec3 color{1, 1, 1};
  double opacity{1.0};
  vec2 texpos{0, 0};
};

// ============================================================================
// TRIANGLE (custom geometry primitive)
// ============================================================================

struct triangle_object : object_base
{
  vertex_data m_v0;
  vertex_data m_v1;
  vertex_data m_v2;
};

template<>
struct object_params<triangle_object>
{
  static constexpr auto value = std::tuple{};
};

namespace build
{
template<typename... Binders>
triangle_object triangle(Binders... binders)
{
  return make<triangle_object>(binders...);
}
} // namespace build

// ============================================================================
// QUAD (custom geometry primitive - 4 vertices)
// ============================================================================

struct quad_object : object_base
{
  vertex_data m_v0;
  vertex_data m_v1;
  vertex_data m_v2;
  vertex_data m_v3;
};

template<>
struct object_params<quad_object>
{
  static constexpr auto value = std::tuple{};
};

namespace build
{
template<typename... Binders>
quad_object quad(Binders... binders)
{
  return make<quad_object>(binders...);
}
} // namespace build

// ============================================================================
// COMPOUND (merged mesh from multiple objects)
// ============================================================================

struct compound_object : object_base
{
  std::vector<float> m_vertices;   // Packed vertex data
  std::vector<std::uint32_t> m_indices; // Index buffer
  mutable bool m_geometry_dirty{true};

  std::size_t vertex_count() const { return m_vertices.size() / 8; } // 8 floats per vertex
  std::size_t index_count() const { return m_indices.size(); }
};

template<>
struct object_params<compound_object>
{
  static constexpr auto value = std::tuple{};
};

// Helper to convert mesh_data to compound storage format
namespace detail
{
inline void add_mesh_to_compound(compound_object& comp, const mesh::mesh_data& mesh_d, const vec3& obj_color)
{
  std::uint32_t base_vertex = static_cast<std::uint32_t>(comp.m_vertices.size() / 8);

  // Add vertices (position[3] + normal[3] + uv[2] = 8 floats per vertex)
  for (const auto& v : mesh_d.vertices)
  {
    comp.m_vertices.push_back(v.position[0]);
    comp.m_vertices.push_back(v.position[1]);
    comp.m_vertices.push_back(v.position[2]);
    comp.m_vertices.push_back(v.normal[0]);
    comp.m_vertices.push_back(v.normal[1]);
    comp.m_vertices.push_back(v.normal[2]);
    comp.m_vertices.push_back(v.uv[0]);
    comp.m_vertices.push_back(v.uv[1]);
  }

  // Add indices with offset
  for (std::uint32_t idx : mesh_d.indices)
  {
    comp.m_indices.push_back(base_vertex + idx);
  }
}

// Add sphere to compound
inline void add_sphere_to_compound(compound_object& comp, const sphere_object& s)
{
  auto mesh_d = mesh::object_mesh::generate_sphere_mesh(s.m_pos, s.m_radius, s.m_axis, s.m_up);
  add_mesh_to_compound(comp, mesh_d, s.m_color);
}

// Add box to compound
inline void add_box_to_compound(compound_object& comp, const box_object& b)
{
  auto mesh_d = mesh::object_mesh::generate_box_mesh(b.m_pos, b.m_length, b.m_height, b.m_width, b.m_axis, b.m_up);
  add_mesh_to_compound(comp, mesh_d, b.m_color);
}

// Add cylinder to compound
inline void add_cylinder_to_compound(compound_object& comp, const cylinder_object& c)
{
  auto mesh_d = mesh::object_mesh::generate_cylinder_mesh(c.m_pos, c.m_radius, c.m_length, c.m_axis, c.m_up);
  add_mesh_to_compound(comp, mesh_d, c.m_color);
}

// Add cone to compound
inline void add_cone_to_compound(compound_object& comp, const cone_object& co)
{
  auto mesh_d = mesh::object_mesh::generate_cone_mesh(co.m_pos, co.m_radius, co.m_length, co.m_axis, co.m_up);
  add_mesh_to_compound(comp, mesh_d, co.m_color);
}

// Add pyramid to compound
inline void add_pyramid_to_compound(compound_object& comp, const pyramid_object& p)
{
  auto mesh_d = mesh::object_mesh::generate_pyramid_mesh(p.m_pos, p.m_length, p.m_height, p.m_width, p.m_axis, p.m_up);
  add_mesh_to_compound(comp, mesh_d, p.m_color);
}

// Generic add object (uses overloading)
inline void add_object_to_compound(compound_object& comp, const sphere_object& obj) { add_sphere_to_compound(comp, obj); }
inline void add_object_to_compound(compound_object& comp, const box_object& obj) { add_box_to_compound(comp, obj); }
inline void add_object_to_compound(compound_object& comp, const cylinder_object& obj)
{
  add_cylinder_to_compound(comp, obj);
}
inline void add_object_to_compound(compound_object& comp, const cone_object& obj) { add_cone_to_compound(comp, obj); }
inline void add_object_to_compound(compound_object& comp, const pyramid_object& obj)
{
  add_pyramid_to_compound(comp, obj);
}

} // namespace detail

// Create compound from multiple objects
namespace build
{
template<typename... Objects>
compound_object compound(Objects&&... objs)
{
  compound_object comp{};

  // Fold expression to add each object
  (detail::add_object_to_compound(comp, std::forward<Objects>(objs)), ...);

  comp.m_geometry_dirty = true;
  return comp;
}
} // namespace build

// ============================================================================
// TEXT3D (extruded 3D text)
// ============================================================================

struct text3d_object : object_base
{
  std::string m_text;
  double m_height{1.0};          // Text height
  double m_depth{0.2};           // Extrusion depth
  std::string m_font{"sans-serif"};
  std::string m_align{"left"};   // left, center, right
  mutable bool m_geometry_dirty{true};
  mutable std::vector<float> m_cached_vertices;
  mutable std::vector<std::uint32_t> m_cached_indices;

  // Setters for convenience
  text3d_object& text(const std::string& t)
  {
    m_text = t;
    m_geometry_dirty = true;
    return *this;
  }
  text3d_object& align(const std::string& a)
  {
    m_align = a;
    m_geometry_dirty = true;
    return *this;
  }
};

template<>
struct object_params<text3d_object>
{
  static constexpr auto value = std::tuple{
    param_spec<&text3d_object::m_text, decltype(text)>{}, param_spec<&text3d_object::m_height, decltype(height)>{},
    param_spec<&text3d_object::m_depth, decltype(thickness)>{}, param_spec<&text3d_object::m_font, decltype(font)>{},
    param_spec<&text3d_object::m_align, decltype(align)>{}};
};

namespace build
{
template<typename... Binders>
text3d_object text3d(Binders... binders)
{
  return make<text3d_object>(binders...);
}
} // namespace build

// ============================================================================
// EXTRUSION (2D shape along 3D path)
//
// GlowScript's extrusion. Its surface is built when it is made, centred on its bounding box: pos is that
// centre unless pos is given, which moves the centre there, as for GlowScript's compound.
// ============================================================================

// One contour, a contour followed by its holes, or several of those: GlowScript's three forms of shape
struct extrusion_shape
{
  std::vector<std::vector<std::vector<vec2>>> shapes;

  extrusion_shape() = default;
  extrusion_shape(std::vector<vec2> contour) : shapes{{std::move(contour)}} {}
  extrusion_shape(std::vector<std::vector<vec2>> contours) : shapes{std::move(contours)} {}
  extrusion_shape(std::vector<std::vector<std::vector<vec2>>> s) : shapes(std::move(s)) {}
};

// A value for every point of the path: one number for all, or a list with one per point
struct per_point
{
  std::optional<double> single;
  std::vector<double> values;

  per_point() = default;
  per_point(double v) : single(v) {}
  per_point(std::vector<double> v) : values(std::move(v)) {}

  bool given() const { return single || !values.empty(); }
};

struct extrusion_object : object_base
{
  std::vector<vec3> m_path;
  extrusion_shape m_shape;
  per_point m_scale;
  per_point m_xscale;
  per_point m_yscale;
  per_point m_twist;
  bool m_show_start_face{true};
  bool m_show_end_face{true};
  std::optional<vec3> m_start_normal;
  std::optional<vec3> m_end_normal;
  double m_smooth{0.95};
  std::vector<std::size_t> m_sharp_joints;
  std::vector<std::size_t> m_smooth_joints;

  mesh::mesh_data m_mesh; // the surface, centred on pos
  mutable bool m_geometry_dirty{true};
};

template<>
struct object_params<extrusion_object>
{
  static constexpr auto value =
    std::tuple{param_spec<&extrusion_object::m_path, decltype(path)>{},
               param_spec<&extrusion_object::m_shape, decltype(shape)>{},
               param_spec<&extrusion_object::m_scale, decltype(scale)>{},
               param_spec<&extrusion_object::m_xscale, decltype(xscale)>{},
               param_spec<&extrusion_object::m_yscale, decltype(yscale)>{},
               param_spec<&extrusion_object::m_twist, decltype(twist)>{},
               param_spec<&extrusion_object::m_show_start_face, decltype(show_start_face)>{},
               param_spec<&extrusion_object::m_show_end_face, decltype(show_end_face)>{},
               param_spec<&extrusion_object::m_start_normal, decltype(start_normal)>{},
               param_spec<&extrusion_object::m_end_normal, decltype(end_normal)>{},
               param_spec<&extrusion_object::m_smooth, decltype(smooth)>{},
               param_spec<&extrusion_object::m_sharp_joints, decltype(sharp_joints)>{},
               param_spec<&extrusion_object::m_smooth_joints, decltype(smooth_joints)>{}};
};

namespace detail
{
// A per-point value as a list with one entry per point, checked as GlowScript checks it
inline std::vector<double> per_point_values(const per_point& v, std::size_t n, bool closed, const char* name)
{
  if (v.single)
    return std::vector<double>(n, *v.single);
  if (v.values.size() != n)
    throw std::invalid_argument(
      std::format("vcpp: the extrusion's {} list has {} values for a path of {} points", name, v.values.size(), n));
  if (closed && v.values.back() != v.values.front())
    throw std::invalid_argument(std::format("vcpp: with a closed path, the last {} must equal the first", name));
  return v.values;
}

// Builds the surface from the parameters; with keep_pos false, pos becomes the centre of its bounding box
inline void build_extrusion(extrusion_object& x, bool keep_pos)
{
  mesh::extrusion_spec s;
  s.path = x.m_path;
  s.shapes = x.m_shape.shapes;
  const std::size_t n = s.path.size();
  const bool closed = n > 1 && s.path.back() == s.path.front();
  if (x.m_scale.given())
  {
    if (x.m_xscale.given() || x.m_yscale.given())
      throw std::invalid_argument("vcpp: an extrusion can't take both scale and xscale or yscale");
    s.xscale = per_point_values(x.m_scale, n, closed, "scale");
    for (double& v : s.xscale)
      if (v == 0)
        v = 1e-7; // as GlowScript does
    s.yscale = s.xscale;
  }
  else
  {
    if (x.m_xscale.given())
      s.xscale = per_point_values(x.m_xscale, n, closed, "xscale");
    if (x.m_yscale.given())
      s.yscale = per_point_values(x.m_yscale, n, closed, "yscale");
  }
  // One twist adds that turn at each joint after the first; a list adds its own value at every joint
  if (x.m_twist.single)
    for (std::size_t i = 0; i < n; ++i)
      s.twist.push_back(static_cast<double>(i) * *x.m_twist.single);
  else if (!x.m_twist.values.empty())
  {
    double total = 0;
    for (double t : per_point_values(x.m_twist, n, false, "twist"))
      s.twist.push_back(total += t);
  }
  s.show_start_face = x.m_show_start_face;
  s.show_end_face = x.m_show_end_face;
  s.start_normal = x.m_start_normal;
  s.end_normal = x.m_end_normal;
  s.smooth = x.m_smooth;
  s.sharp_joints = x.m_sharp_joints;
  s.smooth_joints = x.m_smooth_joints;
  s.up = hat(x.m_up);
  x.m_up = vec3{0, 1, 0}; // up orients the shape along the path, not the finished object, as in GlowScript

  x.m_mesh = mesh::generate_extrusion(std::move(s));
  std::array<double, 3> lo;
  std::array<double, 3> hi;
  lo.fill(std::numeric_limits<double>::infinity());
  hi.fill(-std::numeric_limits<double>::infinity());
  for (const auto& v : x.m_mesh.vertices)
    for (std::size_t i = 0; i < 3; ++i)
    {
      lo[i] = std::min(lo[i], double{v.position[i]});
      hi[i] = std::max(hi[i], double{v.position[i]});
    }
  const std::array<double, 3> centre{(lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2};
  for (auto& v : x.m_mesh.vertices)
    for (std::size_t i = 0; i < 3; ++i)
      v.position[i] -= static_cast<float>(centre[i]);
  if (!keep_pos)
    x.m_pos = vec3{centre[0], centre[1], centre[2]};
}
} // namespace detail

namespace build
{
template<typename... Binders>
extrusion_object extrusion(Binders... binders)
{
  extrusion_object x = make<extrusion_object>(binders...);
  constexpr bool pos_given = (std::same_as<typename Binders::symbol_type, std::remove_cvref_t<decltype(pos)>> || ...);
  detail::build_extrusion(x, pos_given);
  return x;
}
} // namespace build

} // namespace vcpp
