/*
 *  vcpp:pick - Where a ray from the camera meets each kind of object, for scene.mouse.pick()
 *
 *  GlowScript picks by drawing every object in an ID colour and reading the pixel under the mouse; vcpp casts a
 *  ray through it instead (decided with Colin, 2026-10-08). The two agree except at a pixel where objects meet.
 *  Each test returns the ray parameter t of the nearest point in front of the camera, so tests of different
 *  objects compare. Shapes are tested in their own frame, scaled to a unit shape, which keeps t unchanged.
 */

module;

import std;

export module vcpp:pick;

import :vec;
import :objects;

export namespace vcpp::picking
{

struct ray
{
  vec3 origin;
  vec3 dir;
};

// A ray in a shape's frame: origin at `at`, axes x, y, z of the orientation, each divided by scale
inline ray to_local(const ray& r, const vec3& at, const orientation& f, const vec3& scale)
{
  const vec3 p = r.origin - at;
  return {vec3{dot(p, f.x) / scale.x(), dot(p, f.y) / scale.y(), dot(p, f.z) / scale.z()},
          vec3{dot(r.dir, f.x) / scale.x(), dot(r.dir, f.y) / scale.y(), dot(r.dir, f.z) / scale.z()}};
}

inline bool flat(const vec3& scale) { return !(scale.x() > 0 && scale.y() > 0 && scale.z() > 0); }

// The nearest t >= 0 at which the ray is inside the solid where n.p <= c for every plane; a ray starting
// inside meets it where it leaves
struct plane
{
  vec3 n;
  double c;
};

inline std::optional<double> convex(const ray& r, std::span<const plane> planes)
{
  double t_in = -std::numeric_limits<double>::infinity();
  double t_out = std::numeric_limits<double>::infinity();
  for (const plane& p : planes)
  {
    const double denom = dot(p.n, r.dir);
    const double room = p.c - dot(p.n, r.origin);
    if (denom == 0)
    {
      if (room < 0)
        return std::nullopt;
      continue;
    }
    const double t = room / denom;
    if (denom > 0)
      t_out = std::min(t_out, t);
    else
      t_in = std::max(t_in, t);
  }
  if (t_in > t_out || t_out < 0)
    return std::nullopt;
  return t_in >= 0 ? t_in : t_out;
}

// Smallest root t >= 0 of a t^2 + b t + c = 0 for which accept(t) holds
template<typename Accept>
std::optional<double> quadratic(double a, double b, double c, Accept accept)
{
  if (a == 0)
    return std::nullopt;
  const double disc = b * b - 4 * a * c;
  if (disc < 0)
    return std::nullopt;
  const double s = std::sqrt(disc);
  double t0 = (-b - s) / (2 * a);
  double t1 = (-b + s) / (2 * a);
  if (t0 > t1)
    std::swap(t0, t1);
  for (double t : {t0, t1})
    if (t >= 0 && accept(t))
      return t;
  return std::nullopt;
}

inline std::optional<double> nearest(std::optional<double> a, std::optional<double> b)
{
  if (!a)
    return b;
  if (!b)
    return a;
  return std::min(*a, *b);
}

// Unit shapes in their own frame (the frames the renderer's meshes use)
// Sphere of radius 0.5 about the origin
inline std::optional<double> unit_sphere(const ray& r)
{
  return quadratic(dot(r.dir, r.dir), 2 * dot(r.origin, r.dir), dot(r.origin, r.origin) - 0.25,
                   [](double) { return true; });
}

// Box from -0.5 to 0.5 on each axis
inline std::optional<double> unit_box(const ray& r)
{
  static constexpr std::array<plane, 6> planes{{{vec3{1, 0, 0}, 0.5},
                                                {vec3{-1, 0, 0}, 0.5},
                                                {vec3{0, 1, 0}, 0.5},
                                                {vec3{0, -1, 0}, 0.5},
                                                {vec3{0, 0, 1}, 0.5},
                                                {vec3{0, 0, -1}, 0.5}}};
  return convex(r, planes);
}

// Pyramid with its 1 x 1 base at x = 0 and its apex at x = 1
inline std::optional<double> unit_pyramid(const ray& r)
{
  static constexpr std::array<plane, 5> planes{{{vec3{-1, 0, 0}, 0},
                                                {vec3{0.5, 1, 0}, 0.5},
                                                {vec3{0.5, -1, 0}, 0.5},
                                                {vec3{0.5, 0, 1}, 0.5},
                                                {vec3{0.5, 0, -1}, 0.5}}};
  return convex(r, planes);
}

// Where the ray crosses the plane x = at inside radius `radius` of the x axis
inline std::optional<double> disc_at(const ray& r, double at, double radius)
{
  if (r.dir.x() == 0)
    return std::nullopt;
  const double t = (at - r.origin.x()) / r.dir.x();
  const vec3 p = r.origin + r.dir * t;
  if (t < 0 || p.y() * p.y() + p.z() * p.z() > radius * radius)
    return std::nullopt;
  return t;
}

// Cylinder of radius 0.5 from x = 0 to x = 1
inline std::optional<double> unit_cylinder(const ray& r)
{
  const double a = r.dir.y() * r.dir.y() + r.dir.z() * r.dir.z();
  const double b = 2 * (r.origin.y() * r.dir.y() + r.origin.z() * r.dir.z());
  const double c = r.origin.y() * r.origin.y() + r.origin.z() * r.origin.z() - 0.25;
  const auto side = quadratic(a, b, c, [&](double t) {
    const double x = r.origin.x() + r.dir.x() * t;
    return x >= 0 && x <= 1;
  });
  return nearest(side, nearest(disc_at(r, 0, 0.5), disc_at(r, 1, 0.5)));
}

// Cone with a base of radius 0.5 at x = 0 and its apex at x = 1
inline std::optional<double> unit_cone(const ray& r)
{
  // y^2 + z^2 = (0.5 (1 - x))^2
  const double kx = 0.5 * r.dir.x();
  const double k0 = 0.5 * (1 - r.origin.x());
  const double a = r.dir.y() * r.dir.y() + r.dir.z() * r.dir.z() - kx * kx;
  const double b = 2 * (r.origin.y() * r.dir.y() + r.origin.z() * r.dir.z() + k0 * kx);
  const double c = r.origin.y() * r.origin.y() + r.origin.z() * r.origin.z() - k0 * k0;
  const auto side = quadratic(a, b, c, [&](double t) {
    const double x = r.origin.x() + r.dir.x() * t;
    return x >= 0 && x <= 1;
  });
  return nearest(side, disc_at(r, 0, 0.5));
}

// The nearest t >= 0 where the ray meets triangle a b c (Moller-Trumbore)
inline std::optional<double> triangle(const ray& r, const vec3& a, const vec3& b, const vec3& c)
{
  const vec3 e1 = b - a;
  const vec3 e2 = c - a;
  const vec3 p = cross(r.dir, e2);
  const double det = dot(e1, p);
  if (std::abs(det) < 1e-300)
    return std::nullopt;
  const vec3 s = r.origin - a;
  const double u = dot(s, p) / det;
  if (u < 0 || u > 1)
    return std::nullopt;
  const vec3 q = cross(s, e1);
  const double v = dot(r.dir, q) / det;
  if (v < 0 || u + v > 1)
    return std::nullopt;
  const double t = dot(e2, q) / det;
  return t >= 0 ? std::optional{t} : std::nullopt;
}

// A shape of the given unit kind at `at`, turned by axis and up, scaled
template<typename Unit>
std::optional<double> shape(const ray& r, Unit unit, const vec3& at, const vec3& axis, const vec3& up,
                            const vec3& scale)
{
  if (flat(scale) || mag2(axis) == 0)
    return std::nullopt;
  return unit(to_local(r, at, orientation_of(axis, up), scale));
}

// A cylinder of `radius` from a to b
inline std::optional<double> rod(const ray& r, const vec3& a, const vec3& b, double radius)
{ return shape(r, unit_cylinder, a, b - a, vec3{0, 1, 0}, vec3{mag(b - a), 2 * radius, 2 * radius}); }

// ----------------------------------------------------------------------------- objects, as the renderer draws them

inline std::optional<double> hit(const sphere_object& s, const ray& r)
{
  const double d = 2 * s.m_radius;
  return shape(r, unit_sphere, s.m_pos, s.m_axis, s.m_up, vec3{d, d, d});
}

inline std::optional<double> hit(const ellipsoid_object& e, const ray& r)
{ return shape(r, unit_sphere, e.m_pos, e.m_axis, e.m_up, vec3{e.m_length, e.m_height, e.m_width}); }

inline std::optional<double> hit(const box_object& b, const ray& r)
{ return shape(r, unit_box, b.m_pos, b.m_axis, b.m_up, vec3{b.m_length, b.m_height, b.m_width}); }

inline std::optional<double> hit(const pyramid_object& p, const ray& r)
{ return shape(r, unit_pyramid, p.m_pos, p.m_axis, p.m_up, vec3{p.m_length, p.m_height, p.m_width}); }

inline std::optional<double> hit(const cylinder_object& c, const ray& r)
{
  const double d = 2 * c.m_radius;
  return shape(r, unit_cylinder, c.m_pos, c.m_axis, c.m_up, vec3{mag(c.m_axis), d, d});
}

inline std::optional<double> hit(const cone_object& c, const ray& r)
{
  const double d = 2 * c.m_radius;
  return shape(r, unit_cone, c.m_pos, c.m_axis, c.m_up, vec3{mag(c.m_axis), d, d});
}

inline std::optional<double> hit(const arrow_object& a, const ray& r)
{
  const auto parts = arrow_parts(a);
  if (!parts)
    return std::nullopt;
  const auto& [shaft, head] = *parts;
  const vec3 shaft_size{shaft.length, shaft.width, shaft.width};
  const vec3 head_size{head.length, head.width, head.width};
  if (a.m_round)
    return nearest(shape(r, unit_cylinder, shaft.pos, a.m_axis, a.m_up, shaft_size),
                   shape(r, unit_cone, head.pos, a.m_axis, a.m_up, head_size));
  return nearest(shape(r, unit_box, shaft.pos, a.m_axis, a.m_up, shaft_size),
                 shape(r, unit_pyramid, head.pos, a.m_axis, a.m_up, head_size));
}

inline std::optional<double> hit(const triangle_object& tri, const ray& r)
{ return triangle(r, tri.m_v0.pos, tri.m_v1.pos, tri.m_v2.pos); }

inline std::optional<double> hit(const quad_object& q, const ray& r)
{ return nearest(triangle(r, q.m_v0.pos, q.m_v1.pos, q.m_v2.pos), triangle(r, q.m_v0.pos, q.m_v2.pos, q.m_v3.pos)); }

// Each point as drawn: a sphere `size` hundredths of a unit across (see known issue points-size-units)
inline std::optional<double> hit(const points_object& pts, const ray& r)
{
  std::optional<double> best;
  const double d = pts.m_size * 0.01;
  for (const vec3& p : pts.m_points)
    best = nearest(best, unit_sphere(to_local(r, p, orientation{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}, vec3{d, d, d})));
  return best;
}

// A curve's segments, each a rod of the larger of its two ends' radii; `segment` is the index of the point that
// ends it, as GlowScript's hit.segment. A radius of 0 is drawn `thin_radius` wide.
struct curve_hit
{
  double t;
  std::size_t segment;
};

inline std::optional<curve_hit> hit(const curve_object& crv, const ray& r, double thin_radius)
{
  std::optional<curve_hit> best;
  auto radius_of = [&](const curve_point& p) {
    return p.radius > 0 ? p.radius : crv.m_radius > 0 ? crv.m_radius : thin_radius;
  };
  for (std::size_t i = 1; i < crv.m_points.size(); ++i)
  {
    const curve_point& a = crv.m_points[i - 1];
    const curve_point& b = crv.m_points[i];
    if (const auto t = rod(r, a.pos, b.pos, std::max(radius_of(a), radius_of(b))); t && (!best || *t < best->t))
      best = curve_hit{*t, i};
  }
  return best;
}

} // namespace vcpp::picking
