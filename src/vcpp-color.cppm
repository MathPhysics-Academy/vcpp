/*
 *  vcpp:color - Color constants and utilities
 *
 *  Mirrors VPython's color API with named colors and conversion functions.
 */

module;

import std;

export module vcpp:color;

import :vec;

export namespace vcpp
{

// ============================================================================
// Color Namespace (matches VPython's color.red, color.blue, etc.)
// ============================================================================

namespace colors
{

// Primary colors
inline constexpr vec3 white{1.0, 1.0, 1.0};
inline constexpr vec3 black{0.0, 0.0, 0.0};
inline constexpr vec3 red{1.0, 0.0, 0.0};
inline constexpr vec3 green{0.0, 1.0, 0.0};
inline constexpr vec3 blue{0.0, 0.0, 1.0};

// Secondary colors
inline constexpr vec3 yellow{1.0, 1.0, 0.0};
inline constexpr vec3 cyan{0.0, 1.0, 1.0};
inline constexpr vec3 magenta{1.0, 0.0, 1.0};

// Additional VPython colors
inline constexpr vec3 orange{1.0, 0.6, 0.0};
inline constexpr vec3 purple{0.4, 0.2, 0.6};

// Grayscale helper
constexpr vec3 gray(double level) noexcept { return vec3{level, level, level}; }

constexpr vec3 grey(double level) noexcept { return gray(level); }

// ============================================================================
// HSV <-> RGB Conversion
// ============================================================================

// Convert HSV (hue [0-1], saturation [0-1], value [0-1]) to RGB
constexpr vec3 hsv_to_rgb(const vec3& hsv) noexcept
{
  double h = hsv.x();
  double s = hsv.y();
  double v = hsv.z();

  if (s == 0.0)
  {
    return vec3{v, v, v}; // Achromatic (gray)
  }

  // Normalize hue to [0, 1) and scale to [0, 6)
  h = std::fmod(h, 1.0);
  if (h < 0.0)
    h += 1.0;
  h *= 6.0;

  int i = static_cast<int>(h);
  double f = h - i;
  double p = v * (1.0 - s);
  double q = v * (1.0 - s * f);
  double t = v * (1.0 - s * (1.0 - f));

  switch (i % 6)
  {
    case 0:
      return vec3{v, t, p};
    case 1:
      return vec3{q, v, p};
    case 2:
      return vec3{p, v, t};
    case 3:
      return vec3{p, q, v};
    case 4:
      return vec3{t, p, v};
    default:
      return vec3{v, p, q};
  }
}

// Convert RGB to HSV
constexpr vec3 rgb_to_hsv(const vec3& rgb) noexcept
{
  double r = rgb.x();
  double g = rgb.y();
  double b = rgb.z();

  double max_c = std::max({r, g, b});
  double min_c = std::min({r, g, b});
  double delta = max_c - min_c;

  double h = 0.0;
  double s = 0.0;
  double v = max_c;

  if (delta != 0.0)
  {
    s = delta / max_c;

    if (r == max_c)
    {
      h = (g - b) / delta;
    }
    else if (g == max_c)
    {
      h = 2.0 + (b - r) / delta;
    }
    else
    {
      h = 4.0 + (r - g) / delta;
    }

    h /= 6.0;
    if (h < 0.0)
      h += 1.0;
  }

  return vec3{h, s, v};
}

// ============================================================================
// Color Manipulation
// ============================================================================

// Blend two colors
constexpr vec3 blend(const vec3& a, const vec3& b, double t) noexcept
{
  return vec3{a.x() + (b.x() - a.x()) * t, a.y() + (b.y() - a.y()) * t, a.z() + (b.z() - a.z()) * t};
}

// Adjust brightness
constexpr vec3 brighten(const vec3& c, double factor) noexcept
{
  return vec3{std::clamp(c.x() * factor, 0.0, 1.0), std::clamp(c.y() * factor, 0.0, 1.0),
              std::clamp(c.z() * factor, 0.0, 1.0)};
}

// Invert color
constexpr vec3 invert(const vec3& c) noexcept { return vec3{1.0 - c.x(), 1.0 - c.y(), 1.0 - c.z()}; }

} // namespace colors

// ============================================================================
// Top-level Color Constants (for convenience)
// Allows: sphere(color = red) instead of sphere(color = colors::red)
// ============================================================================

inline constexpr vec3 white = colors::white;
inline constexpr vec3 black = colors::black;
inline constexpr vec3 red = colors::red;
inline constexpr vec3 green = colors::green;
inline constexpr vec3 blue = colors::blue;
inline constexpr vec3 yellow = colors::yellow;
inline constexpr vec3 cyan = colors::cyan;
inline constexpr vec3 magenta = colors::magenta;
inline constexpr vec3 orange = colors::orange;
inline constexpr vec3 purple = colors::purple;

} // namespace vcpp
