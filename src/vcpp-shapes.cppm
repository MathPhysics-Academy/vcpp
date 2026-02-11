/*
 *  vcpp:shapes - 2D shape definitions for extrusions
 *
 *  Provides:
 *  - shape2d: 2D shape container with outer contour and optional holes
 *  - Predefined shapes: circle, rectangle, ngon, star, etc.
 */

module;

import std;

export module vcpp:shapes;

import :vec;

export namespace vcpp::shapes
{

// ============================================================================
// shape2d - 2D shape definition
//
// Contains an outer contour as a vector of vec2 points.
// Points should be in counter-clockwise order for correct normals.
// ============================================================================

struct shape2d
{
  std::vector<vec2> outer;
  std::vector<std::vector<vec2>> holes; // Optional holes (clockwise order)

  // Check if shape is valid
  bool valid() const noexcept { return outer.size() >= 3; }

  // Get point count
  std::size_t point_count() const noexcept { return outer.size(); }

  // Convert to simple vector for extrusion
  const std::vector<vec2>& points() const noexcept { return outer; }
};

// ============================================================================
// Circle shape
//
// Creates a circle with the specified radius and number of segments.
// ============================================================================

inline shape2d circle(double radius = 1.0, int segments = 32)
{
  shape2d shape;
  shape.outer.reserve(segments);

  constexpr double TWO_PI = 2.0 * 3.14159265358979323846;

  for (int i = 0; i < segments; ++i)
  {
    double angle = TWO_PI * static_cast<double>(i) / static_cast<double>(segments);
    shape.outer.push_back(vec2{radius * std::cos(angle), radius * std::sin(angle)});
  }

  return shape;
}

// ============================================================================
// Rectangle shape
//
// Creates a rectangle centered at origin with given width and height.
// ============================================================================

inline shape2d rectangle(double width = 1.0, double height = 1.0)
{
  shape2d shape;
  double hw = width / 2.0;
  double hh = height / 2.0;

  // Counter-clockwise
  shape.outer = {
    vec2{-hw, -hh},
    vec2{hw, -hh},
    vec2{hw, hh},
    vec2{-hw, hh},
  };

  return shape;
}

// Alias for rectangle
inline shape2d rect(double width = 1.0, double height = 1.0) { return rectangle(width, height); }

// ============================================================================
// Square shape
//
// Creates a square centered at origin with given size.
// ============================================================================

inline shape2d square(double size = 1.0) { return rectangle(size, size); }

// ============================================================================
// Regular N-gon shape
//
// Creates a regular polygon with n sides inscribed in a circle of given radius.
// ============================================================================

inline shape2d ngon(int n, double radius = 1.0)
{
  if (n < 3)
    n = 3;

  shape2d shape;
  shape.outer.reserve(n);

  constexpr double TWO_PI = 2.0 * 3.14159265358979323846;

  for (int i = 0; i < n; ++i)
  {
    double angle = TWO_PI * static_cast<double>(i) / static_cast<double>(n);
    shape.outer.push_back(vec2{radius * std::cos(angle), radius * std::sin(angle)});
  }

  return shape;
}

// ============================================================================
// Triangle shape
//
// Creates an equilateral triangle inscribed in a circle of given radius.
// ============================================================================

inline shape2d triangle(double radius = 1.0) { return ngon(3, radius); }

// ============================================================================
// Pentagon shape
// ============================================================================

inline shape2d pentagon(double radius = 1.0) { return ngon(5, radius); }

// ============================================================================
// Hexagon shape
// ============================================================================

inline shape2d hexagon(double radius = 1.0) { return ngon(6, radius); }

// ============================================================================
// Star shape
//
// Creates a star with the specified number of points.
// outer_radius is the tip radius, inner_radius is the valley radius.
// ============================================================================

inline shape2d star(int points = 5, double outer_radius = 1.0, double inner_radius = 0.5)
{
  if (points < 3)
    points = 3;

  shape2d shape;
  shape.outer.reserve(points * 2);

  constexpr double TWO_PI = 2.0 * 3.14159265358979323846;
  double angle_step = TWO_PI / static_cast<double>(points * 2);

  for (int i = 0; i < points * 2; ++i)
  {
    double angle = angle_step * static_cast<double>(i) - 3.14159265358979323846 / 2.0;
    double r = (i % 2 == 0) ? outer_radius : inner_radius;
    shape.outer.push_back(vec2{r * std::cos(angle), r * std::sin(angle)});
  }

  return shape;
}

// ============================================================================
// Ellipse shape
//
// Creates an ellipse with the specified semi-axes.
// ============================================================================

inline shape2d ellipse(double a = 1.0, double b = 0.5, int segments = 32)
{
  shape2d shape;
  shape.outer.reserve(segments);

  constexpr double TWO_PI = 2.0 * 3.14159265358979323846;

  for (int i = 0; i < segments; ++i)
  {
    double angle = TWO_PI * static_cast<double>(i) / static_cast<double>(segments);
    shape.outer.push_back(vec2{a * std::cos(angle), b * std::sin(angle)});
  }

  return shape;
}

// ============================================================================
// Rounded rectangle shape
//
// Creates a rectangle with rounded corners.
// ============================================================================

inline shape2d rounded_rectangle(double width = 1.0, double height = 0.5, double corner_radius = 0.1,
                                 int corner_segments = 8)
{
  shape2d shape;

  double hw = width / 2.0 - corner_radius;
  double hh = height / 2.0 - corner_radius;

  if (hw < 0)
    hw = 0;
  if (hh < 0)
    hh = 0;

  constexpr double HALF_PI = 3.14159265358979323846 / 2.0;

  auto add_corner = [&](double cx, double cy, double start_angle) {
    for (int i = 0; i <= corner_segments; ++i)
    {
      double angle = start_angle + HALF_PI * static_cast<double>(i) / static_cast<double>(corner_segments);
      shape.outer.push_back(vec2{cx + corner_radius * std::cos(angle), cy + corner_radius * std::sin(angle)});
    }
  };

  // Bottom-right corner
  add_corner(hw, -hh, -HALF_PI);
  // Top-right corner
  add_corner(hw, hh, 0);
  // Top-left corner
  add_corner(-hw, hh, HALF_PI);
  // Bottom-left corner
  add_corner(-hw, -hh, 3.14159265358979323846);

  return shape;
}

// ============================================================================
// Cross shape
//
// Creates a plus/cross shape.
// ============================================================================

inline shape2d cross(double size = 1.0, double thickness = 0.3)
{
  shape2d shape;
  double hs = size / 2.0;
  double ht = thickness / 2.0;

  // 12 vertices forming a cross, counter-clockwise
  shape.outer = {
    vec2{-ht, -hs}, vec2{ht, -hs},  vec2{ht, -ht},  vec2{hs, -ht},
    vec2{hs, ht},   vec2{ht, ht},   vec2{ht, hs},   vec2{-ht, hs},
    vec2{-ht, ht},  vec2{-hs, ht},  vec2{-hs, -ht}, vec2{-ht, -ht},
  };

  return shape;
}

// ============================================================================
// Arrow shape
//
// Creates a 2D arrow shape pointing in +Y direction.
// ============================================================================

inline shape2d arrow(double length = 1.0, double shaft_width = 0.3, double head_width = 0.6, double head_length = 0.4)
{
  shape2d shape;
  double hsw = shaft_width / 2.0;
  double hhw = head_width / 2.0;
  double shaft_len = length - head_length;

  // Counter-clockwise from bottom-left of shaft
  shape.outer = {
    vec2{-hsw, 0},
    vec2{hsw, 0},
    vec2{hsw, shaft_len},
    vec2{hhw, shaft_len},
    vec2{0, length}, // tip
    vec2{-hhw, shaft_len},
    vec2{-hsw, shaft_len},
  };

  return shape;
}

// ============================================================================
// L-shape
//
// Creates an L-shaped profile.
// ============================================================================

inline shape2d l_shape(double width = 1.0, double height = 1.0, double thickness = 0.2)
{
  shape2d shape;

  // Outer L shape, counter-clockwise
  shape.outer = {
    vec2{0, 0}, vec2{width, 0}, vec2{width, thickness}, vec2{thickness, thickness},
    vec2{thickness, height}, vec2{0, height},
  };

  return shape;
}

// ============================================================================
// T-shape
//
// Creates a T-shaped profile.
// ============================================================================

inline shape2d t_shape(double width = 1.0, double height = 1.0, double thickness = 0.2)
{
  shape2d shape;
  double hw = width / 2.0;
  double ht = thickness / 2.0;

  shape.outer = {
    vec2{-ht, 0},
    vec2{ht, 0},
    vec2{ht, height - thickness},
    vec2{hw, height - thickness},
    vec2{hw, height},
    vec2{-hw, height},
    vec2{-hw, height - thickness},
    vec2{-ht, height - thickness},
  };

  return shape;
}

// ============================================================================
// I-beam shape
//
// Creates an I-beam profile.
// ============================================================================

inline shape2d i_beam(double width = 1.0, double height = 1.0, double flange_thickness = 0.2, double web_thickness = 0.1)
{
  shape2d shape;
  double hw = width / 2.0;
  double hwt = web_thickness / 2.0;

  shape.outer = {
    vec2{-hw, 0},
    vec2{hw, 0},
    vec2{hw, flange_thickness},
    vec2{hwt, flange_thickness},
    vec2{hwt, height - flange_thickness},
    vec2{hw, height - flange_thickness},
    vec2{hw, height},
    vec2{-hw, height},
    vec2{-hw, height - flange_thickness},
    vec2{-hwt, height - flange_thickness},
    vec2{-hwt, flange_thickness},
    vec2{-hw, flange_thickness},
  };

  return shape;
}

// ============================================================================
// Gear shape
//
// Creates a simple gear profile with the specified number of teeth.
// ============================================================================

inline shape2d gear(int teeth = 12, double outer_radius = 1.0, double inner_radius = 0.8, double tooth_width_ratio = 0.5)
{
  if (teeth < 3)
    teeth = 3;

  shape2d shape;
  constexpr double TWO_PI = 2.0 * 3.14159265358979323846;

  double angle_per_tooth = TWO_PI / static_cast<double>(teeth);
  double tooth_angle = angle_per_tooth * tooth_width_ratio;
  double gap_angle = angle_per_tooth * (1.0 - tooth_width_ratio);

  for (int i = 0; i < teeth; ++i)
  {
    double base_angle = angle_per_tooth * static_cast<double>(i);

    // Inner point before tooth
    shape.outer.push_back(vec2{inner_radius * std::cos(base_angle), inner_radius * std::sin(base_angle)});

    // Outer tooth start
    double tooth_start = base_angle + gap_angle / 2.0;
    shape.outer.push_back(vec2{outer_radius * std::cos(tooth_start), outer_radius * std::sin(tooth_start)});

    // Outer tooth end
    double tooth_end = tooth_start + tooth_angle;
    shape.outer.push_back(vec2{outer_radius * std::cos(tooth_end), outer_radius * std::sin(tooth_end)});

    // Inner point after tooth
    double next_base = base_angle + angle_per_tooth;
    shape.outer.push_back(vec2{inner_radius * std::cos(next_base - 0.001), inner_radius * std::sin(next_base - 0.001)});
  }

  return shape;
}

} // namespace vcpp::shapes
