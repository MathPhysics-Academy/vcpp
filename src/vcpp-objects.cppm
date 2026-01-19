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

export namespace vcpp
{

using namespace lam::symbols;

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

template<>
struct object_params<sphere_object>
{
  static constexpr auto value = std::tuple{param_spec<&sphere_object::m_radius, decltype(radius), 1.0>{}};
};

template<typename... Binders>
constexpr sphere_object sphere(Binders... binders)
{
  return make<sphere_object>(binders...);
}

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
  static constexpr auto value = std::tuple{param_spec<&ellipsoid_object::m_length, decltype(length), 1.0>{},
                                           param_spec<&ellipsoid_object::m_height, decltype(height), 1.0>{},
                                           param_spec<&ellipsoid_object::m_width, decltype(width), 1.0>{}};
};

template<typename... Binders>
constexpr ellipsoid_object ellipsoid(Binders... binders)
{
  return make<ellipsoid_object>(binders...);
}

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
  static constexpr auto value = std::tuple{param_spec<&box_object::m_length, decltype(length), 1.0>{},
                                           param_spec<&box_object::m_height, decltype(height), 1.0>{},
                                           param_spec<&box_object::m_width, decltype(width), 1.0>{}};
};

template<typename... Binders>
constexpr box_object box(Binders... binders)
{
  return make<box_object>(binders...);
}

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
  static constexpr auto value = std::tuple{param_spec<&cylinder_object::m_radius, decltype(radius), 1.0>{},
                                           param_spec<&cylinder_object::m_length, decltype(length), 1.0>{}};
};

template<typename... Binders>
constexpr cylinder_object cylinder(Binders... binders)
{
  return make<cylinder_object>(binders...);
}

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
  static constexpr auto value = std::tuple{param_spec<&cone_object::m_radius, decltype(radius), 1.0>{},
                                           param_spec<&cone_object::m_length, decltype(length), 1.0>{}};
};

template<typename... Binders>
constexpr cone_object cone(Binders... binders)
{
  return make<cone_object>(binders...);
}

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
  static constexpr auto value = std::tuple{param_spec<&arrow_object::m_shaftwidth, decltype(shaftwidth), 0.1>{},
                                           param_spec<&arrow_object::m_headwidth, decltype(headwidth), 0.2>{},
                                           param_spec<&arrow_object::m_headlength, decltype(headlength), 0.3>{},
                                           param_spec<&arrow_object::m_round, decltype(round), false>{}};
};

template<typename... Binders>
constexpr arrow_object arrow(Binders... binders)
{
  return make<arrow_object>(binders...);
}

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

template<>
struct object_params<ring_object>
{
  static constexpr auto value = std::tuple{param_spec<&ring_object::m_radius, decltype(radius), 1.0>{},
                                           param_spec<&ring_object::m_thickness, decltype(thickness), 0.1>{}};
};

template<typename... Binders>
constexpr ring_object ring(Binders... binders)
{
  return make<ring_object>(binders...);
}

// ============================================================================
// HELIX
// ============================================================================

struct helix_object : object_base
{
  double m_radius{1.0};
  double m_thickness{0.05}; // wire thickness
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
  static constexpr auto value = std::tuple{param_spec<&helix_object::m_radius, decltype(radius), 1.0>{},
                                           param_spec<&helix_object::m_thickness, decltype(thickness), 0.05>{},
                                           param_spec<&helix_object::m_length, decltype(length), 1.0>{},
                                           param_spec<&helix_object::m_coils, decltype(coils), 5>{},
                                           param_spec<&helix_object::m_ccw, decltype(ccw), true>{}};
};

template<typename... Binders>
constexpr helix_object helix(Binders... binders)
{
  return make<helix_object>(binders...);
}

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
  static constexpr auto value = std::tuple{param_spec<&pyramid_object::m_length, decltype(length), 1.0>{},
                                           param_spec<&pyramid_object::m_height, decltype(height), 1.0>{},
                                           param_spec<&pyramid_object::m_width, decltype(width), 1.0>{}};
};

template<typename... Binders>
constexpr pyramid_object pyramid(Binders... binders)
{
  return make<pyramid_object>(binders...);
}

} // namespace vcpp
