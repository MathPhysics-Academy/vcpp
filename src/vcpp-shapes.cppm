/*
 *  vcpp:shapes - GlowScript's shapes and paths libraries (shapespaths.js)
 *
 *  shapes:: functions make 2D outlines for an extrusion's shape: a list of contours, the outline and then
 *  any hole (given a thickness). paths:: functions make 3D paths for an extrusion's path. Both take
 *  GlowScript's named arguments. Names vcpp already has (pos, radius, width, ...) are the same symbols;
 *  the ones only shapes and paths use live in vcpp::shapes, so write shapes::np = 16, or bring them in
 *  with using namespace vcpp::shapes.
 */

module;

import std;

export module vcpp:shapes;

import lam.symbols;
import :vec;
import :props;
import :mesh;

export namespace vcpp::shapes
{

using lam::symbols::symbol;

using vcpp::depth;
using vcpp::height;
using vcpp::length;
using vcpp::pos;
using vcpp::radius;
using vcpp::scale;
using vcpp::thickness;
using vcpp::up;
using vcpp::width;
using vcpp::xscale;
using vcpp::yscale;

inline constexpr symbol<> np{};        // number of points on a circle, arc or ellipse; sides of an ngon
inline constexpr symbol<> rotate{};    // turn about pos, in radians
inline constexpr symbol<> roundness{}; // rounded corners: radius as a fraction of the shortest side
inline constexpr symbol<> invert{};    // rounded corners cut inward
inline constexpr symbol<> angle1{};    // start of a partial circle or arc
inline constexpr symbol<> angle2{};    // end of a partial circle or arc
inline constexpr symbol<> top{};       // trapezoid's top width
inline constexpr symbol<> iradius{};   // star's inner radius
inline constexpr symbol<> n{};         // star's points, gear's teeth
inline constexpr symbol<> start{};     // line start
inline constexpr symbol<> end{};       // line end
inline constexpr symbol<> phi{};       // gear pressure angle, degrees
inline constexpr symbol<> addendum{};  // gear tooth height above the pitch circle
inline constexpr symbol<> dedendum{};  // gear tooth depth below it
inline constexpr symbol<> fradius{};   // gear fillet radius
inline constexpr symbol<> bevel{};     // gear tooth bevel
inline constexpr symbol<> res{};       // gear tooth resolution

using contour = std::vector<vec2>;
using outline = std::vector<contour>; // the outline, then any hole

namespace detail
{
// The arguments of a call, unset until given, as GlowScript's args object
struct args
{
  std::optional<vec2> pos;
  std::optional<vec3> path_pos; // a path's pos
  std::optional<vec3> up;
  std::optional<double> width;
  std::optional<double> height;
  std::optional<double> length;
  std::optional<double> radius;
  std::optional<double> iradius;
  std::optional<double> thickness;
  std::optional<double> rotate;
  std::optional<double> roundness;
  std::optional<bool> invert;
  std::optional<double> scale;
  std::optional<double> xscale;
  std::optional<double> yscale;
  std::optional<double> angle1;
  std::optional<double> angle2;
  std::optional<double> np;
  std::optional<double> n;
  std::optional<double> top;
  std::optional<double> depth;
  std::optional<vec2> start;
  std::optional<vec2> end;
  std::optional<vec3> path_start;
  std::optional<vec3> path_end;
  std::optional<double> phi;
  std::optional<double> addendum;
  std::optional<double> dedendum;
  std::optional<double> fradius;
  std::optional<double> bevel;
  std::optional<double> res;
  std::optional<vec2> corner; // where a partial circle's straight edges meet
  std::optional<contour> points;
  bool path = false; // making a path: an arc has no inner edge
};

template<typename S>
inline constexpr bool always_false = false;

template<typename Binder>
void set(args& a, const Binder& b)
{
  using S = typename Binder::symbol_type;
  using V = typename Binder::value_type;
  auto is = []<typename T>(const T&) { return std::same_as<S, std::remove_cvref_t<T>>; };
  if constexpr (is(vcpp::pos))
  {
    if constexpr (std::convertible_to<V, vec3>)
      a.path_pos = b();
    else if constexpr (std::convertible_to<V, vec2>)
      a.pos = b();
    else
      a.points = contour(b().begin(), b().end()); // shapes::points: the outline itself
  }
  else if constexpr (is(vcpp::up))
    a.up = b();
  else if constexpr (is(vcpp::width))
    a.width = b();
  else if constexpr (is(vcpp::height))
    a.height = b();
  else if constexpr (is(vcpp::length))
    a.length = b();
  else if constexpr (is(vcpp::radius))
    a.radius = b();
  else if constexpr (is(vcpp::thickness))
    a.thickness = b();
  else if constexpr (is(vcpp::scale))
    a.scale = b();
  else if constexpr (is(vcpp::xscale))
    a.xscale = b();
  else if constexpr (is(vcpp::yscale))
    a.yscale = b();
  else if constexpr (is(vcpp::depth))
    a.depth = b();
  else if constexpr (is(shapes::iradius))
    a.iradius = b();
  else if constexpr (is(shapes::rotate))
    a.rotate = b();
  else if constexpr (is(shapes::roundness))
    a.roundness = b();
  else if constexpr (is(shapes::invert))
    a.invert = b();
  else if constexpr (is(shapes::angle1))
    a.angle1 = b();
  else if constexpr (is(shapes::angle2))
    a.angle2 = b();
  else if constexpr (is(shapes::np))
    a.np = b();
  else if constexpr (is(shapes::n))
    a.n = b();
  else if constexpr (is(shapes::top))
    a.top = b();
  else if constexpr (is(shapes::start))
  {
    if constexpr (std::convertible_to<V, vec3>)
      a.path_start = b();
    else
      a.start = b();
  }
  else if constexpr (is(shapes::end))
  {
    if constexpr (std::convertible_to<V, vec3>)
      a.path_end = b();
    else
      a.end = b();
  }
  else if constexpr (is(shapes::phi))
    a.phi = b();
  else if constexpr (is(shapes::addendum))
    a.addendum = b();
  else if constexpr (is(shapes::dedendum))
    a.dedendum = b();
  else if constexpr (is(shapes::fradius))
    a.fradius = b();
  else if constexpr (is(shapes::bevel))
    a.bevel = b();
  else if constexpr (is(shapes::res))
    a.res = b();
  else
    static_assert(always_false<S>, "vcpp: shapes and paths don't take one of the named parameters passed");
}

template<typename... Binders>
args read(const Binders&... binders)
{
  args a;
  (set(a, binders), ...);
  return a;
}

// GlowScript's helpers on lists of [x, y]
inline contour rotatecp(const contour& cp, const vec2& pr, double angle)
{
  const double sinr = std::sin(angle);
  const double cosr = std::cos(angle);
  const double xr = pr.x();
  const double yr = pr.y();
  contour out;
  for (const vec2& p : cp)
    out.push_back(vec2{p.x() * cosr - p.y() * sinr - xr * cosr + yr * sinr + xr,
                       p.x() * sinr + p.y() * cosr - xr * sinr - yr * cosr + yr});
  return out;
}

inline contour scaled(const contour& cp, double xs, double ys)
{
  contour out;
  for (const vec2& p : cp)
    out.push_back(vec2{xs * p.x(), ys * p.y()});
  return out;
}

inline contour addpos(const vec2& pos, contour cp)
{
  for (vec2& p : cp)
    p = vec2{p.x() + pos.x(), p.y() + pos.y()};
  return cp;
}

// Rounds the corners of a closed contour with arcs of nseg points
inline contour roundc(const contour& cps, double roundness, bool invert, int nseg = 16)
{
  using mesh::detail::gs_diff_angle;
  using mesh::detail::gs_norm;
  using mesh::detail::gs_rotate;
  std::vector<vec3> cp;
  for (const vec2& p : cps)
    cp.push_back(vec3{p.x(), p.y(), 0});
  cp.pop_back(); // the final point repeats the first
  const std::size_t lcp = cp.size();
  double vord = 0;
  for (std::size_t i = 0; i < lcp; ++i)
    vord += vcpp::cross(cp[(i + 1) % lcp] - cp[i], cp[(i + 2) % lcp] - cp[(i + 1) % lcp]).z();
  if (vord < 0)
    std::ranges::reverse(cp); // counter-clockwise
  double shortest = 1e200;
  for (std::size_t i = 0; i < lcp; ++i)
    shortest = std::min(shortest, mag(cp[(i + 1) % lcp] - cp[i]));
  const double r = shortest * roundness;

  contour ncp{vec2{0, 0}}; // the first point is set at the end
  double d = 0;
  for (std::size_t i = 0; i < lcp; ++i)
  {
    const vec3 v1 = cp[(i + 1) % lcp] - cp[i % lcp];
    const vec3 v2 = cp[(i + 2) % lcp] - cp[(i + 1) % lcp];
    const double theta = gs_diff_angle(v1, v2);
    d = r * std::tan(theta / 2);
    const vec3 p1 = cp[i] + (v1 - gs_norm(v1) * d);
    const vec3 p2 = cp[(i + 1) % lcp] + gs_norm(v2) * d;
    ncp.push_back(vec2{p1.x(), p1.y()});
    const vec3 nrm = vcpp::cross(gs_norm(v1), gs_norm(v2));
    vec3 center = p1 + gs_norm(vcpp::cross(nrm, v1)) * r;
    vec3 v = p1 - center;
    double dtheta = theta / (nseg + 1);
    if (nrm.z() < 0)
      dtheta = -dtheta;
    if (invert)
    {
      const vec3 c = (p1 + p2) * 0.5;
      center = c + (c - center);
      v = p1 - center;
      dtheta = -dtheta;
    }
    for (int j = 1; j <= nseg; ++j)
    {
      const vec3 q = center + gs_rotate(v, j * dtheta, vec3{0, 0, 1});
      ncp.push_back(vec2{q.x(), q.y()});
    }
    ncp.push_back(vec2{p2.x(), p2.y()});
  }
  const vec3 first = cp[0] + gs_norm(cp[1] - cp[0]) * d;
  ncp[0] = vec2{first.x(), first.y()};
  return ncp;
}

// The rotation, scaling and rounding most shapes end with
inline contour finish(contour cp, args& a, bool rotate = true)
{
  if (rotate && *a.rotate != 0)
    cp = rotatecp(cp, *a.pos, *a.rotate);
  if (*a.scale != 1)
    a.xscale = a.yscale = *a.scale;
  if (*a.xscale != 1 || *a.yscale != 1)
    cp = scaled(cp, *a.xscale, *a.yscale);
  if (a.roundness && *a.roundness > 0)
    cp = roundc(cp, *a.roundness, a.invert.value_or(false));
  return cp;
}

inline void common_defaults(args& a)
{
  if (!a.pos)
    a.pos = vec2{0, 0};
  if (!a.rotate)
    a.rotate = 0;
  if (!a.roundness)
    a.roundness = 0;
  if (!a.invert)
    a.invert = false;
  if (!a.scale)
    a.scale = 1;
  if (!a.xscale)
    a.xscale = 1;
  if (!a.yscale)
    a.yscale = 1;
}

inline outline rectangle(args a);
inline outline trapezoid(args a);
inline outline circle(args a);
inline outline ngon(args a);
inline outline star(args a);

// A frame: the outline and the same shape shrunk inside it, as one outline with a hole
inline outline frame(contour outer, contour inner, args& a)
{
  if (*a.rotate != 0)
  {
    outer = rotatecp(outer, *a.pos, *a.rotate);
    inner = rotatecp(inner, *a.pos, *a.rotate);
  }
  if (*a.scale != 1)
    a.xscale = a.yscale = *a.scale;
  if (*a.xscale != 1 || *a.yscale != 1)
  {
    outer = scaled(outer, *a.xscale, *a.yscale);
    inner = scaled(inner, *a.xscale, *a.yscale);
  }
  if (a.roundness && *a.roundness > 0)
  {
    outer = roundc(outer, *a.roundness, *a.invert);
    inner = roundc(inner, *a.roundness, *a.invert);
  }
  return {outer, inner};
}

inline outline rframe(args a)
{
  common_defaults(a);
  if (!a.width)
    a.width = 1;
  if (!a.height)
    a.height = *a.width;
  const double t = std::min(*a.height, *a.width) * *a.thickness * 2;
  args o;
  o.pos = a.pos;
  o.width = a.width;
  o.height = a.height;
  args i;
  i.pos = a.pos;
  i.width = *a.width - t;
  i.height = *a.height - t;
  // GlowScript scales a rectangle frame whatever scale says, and only by xscale and yscale
  a.scale = 1;
  return frame(rectangle(o)[0], rectangle(i)[0], a);
}

inline outline rectangle(args a)
{
  common_defaults(a);
  if (!a.width)
    a.width = 1;
  if (!a.height)
    a.height = *a.width;
  if (!a.thickness)
    a.thickness = 0;
  if (*a.thickness != 0)
    return rframe(a);
  const double w2 = *a.width / 2;
  const double h2 = *a.height / 2;
  contour cp{{w2, -h2}, {w2, h2}, {-w2, h2}, {-w2, -h2}, {w2, -h2}};
  return {finish(addpos(*a.pos, cp), a)};
}

inline outline cross(args a)
{
  common_defaults(a);
  if (!a.width)
    a.width = 1;
  if (!a.thickness)
    a.thickness = 0.2;
  const double w2 = *a.width / 2;
  const double t2 = *a.thickness / 2;
  contour cp{{w2, -t2},  {w2, t2},   {t2, t2},   {t2, w2},  {-t2, w2}, {-t2, t2}, {-w2, t2},
             {-w2, -t2}, {-t2, -t2}, {-t2, -w2}, {t2, -w2}, {t2, -t2}, {w2, -t2}};
  return {finish(addpos(*a.pos, cp), a)};
}

inline outline trframe(args a)
{
  common_defaults(a);
  const double t = std::min(*a.height, *a.top) * *a.thickness * 2;
  args o;
  o.pos = a.pos;
  o.width = a.width;
  o.height = a.height;
  o.top = a.top;
  const double angle = std::atan((*a.width - *a.top) / 2 / *a.height);
  const double db = t / std::cos(angle);
  args i;
  i.pos = a.pos;
  i.width = *a.width - db - t * std::tan(angle);
  i.height = *a.height - t;
  i.top = *a.top - (db - t * std::tan(angle));
  // GlowScript adds pos to a trapezoid frame twice
  return frame(addpos(*a.pos, trapezoid(o)[0]), addpos(*a.pos, trapezoid(i)[0]), a);
}

inline outline trapezoid(args a)
{
  common_defaults(a);
  if (!a.width)
    a.width = 2;
  if (!a.height)
    a.height = 1;
  if (!a.thickness)
    a.thickness = 0;
  const double w2 = *a.width / 2;
  const double h2 = *a.height / 2;
  if (!a.top)
    a.top = w2;
  const double t2 = *a.top / 2;
  if (*a.thickness != 0)
    return trframe(a);
  contour cp{{w2, -h2}, {t2, h2}, {-t2, h2}, {-w2, -h2}, {w2, -h2}};
  return {finish(addpos(*a.pos, cp), a)};
}

inline constexpr double npdefault = 64;

inline outline circframe(args a)
{
  if (!a.pos)
    a.pos = vec2{0, 0};
  if (!a.radius)
    a.radius = 0.5;
  if (!a.np)
    a.np = npdefault;
  if (!a.scale)
    a.scale = 1;
  if (!a.xscale)
    a.xscale = 1;
  if (!a.yscale)
    a.yscale = 1;
  if (!a.angle1)
    a.angle1 = 0;
  if (!a.angle2)
    a.angle2 = 2 * std::numbers::pi;
  if (!a.rotate)
    a.rotate = 0;
  a.thickness = 0;
  if (!a.iradius)
    a.iradius = *a.radius * 0.8;
  const contour outer = circle(a)[0];
  if (!(*a.angle1 == 0 && *a.angle2 == 2 * std::numbers::pi))
  {
    const double t = *a.radius - *a.iradius;
    const double angle = (*a.angle1 + *a.angle2) / 2;
    const double offset = t / std::sin((*a.angle2 - *a.angle1) / 2);
    a.corner = vec2{a.pos->x() + offset * std::cos(angle), a.pos->y() + offset * std::sin(angle)};
    const double dangle = std::asin(t / *a.iradius);
    a.angle1 = *a.angle1 + dangle;
    a.angle2 = *a.angle2 - dangle;
  }
  a.radius = *a.iradius;
  const contour inner = circle(a)[0];
  a.roundness = 0;
  // GlowScript has circle scale these already; a circle frame scales them again
  return frame(outer, inner, a);
}

inline outline circle(args a)
{
  if (!a.pos)
    a.pos = vec2{0, 0};
  const vec2 corner = a.corner.value_or(*a.pos);
  if (!a.radius)
    a.radius = 0.5;
  if (!a.np)
    a.np = npdefault;
  if (!a.scale)
    a.scale = 1;
  if (!a.xscale)
    a.xscale = 1;
  if (!a.yscale)
    a.yscale = 1;
  if (!a.thickness)
    a.thickness = 0;
  if (!a.angle1)
    a.angle1 = 0;
  if (!a.angle2)
    a.angle2 = 2 * std::numbers::pi;
  if (!a.rotate)
    a.rotate = 0;
  if (*a.thickness > 0)
  {
    a.iradius = *a.radius - *a.radius * *a.thickness;
    return circframe(a);
  }
  const bool partial = *a.angle1 != 0 || *a.angle2 != 2 * std::numbers::pi;
  contour cp;
  if (partial)
    cp.push_back(corner);
  double seg = 2 * std::numbers::pi / *a.np;
  int nseg = static_cast<int>(std::floor(std::abs((*a.angle2 - *a.angle1) / seg + 0.5)));
  seg = (*a.angle2 - *a.angle1) / nseg;
  if (partial)
    nseg += 1;
  double c = *a.radius * std::cos(*a.angle1);
  double s = *a.radius * std::sin(*a.angle1);
  const double dc = std::cos(seg);
  const double ds = std::sin(seg);
  const double x0 = a.pos->x();
  const double y0 = a.pos->y();
  cp.push_back(vec2{x0 + c, y0 + s});
  for (int i = 0; i < nseg - 1; ++i)
  {
    const double c2 = c * dc - s * ds;
    const double s2 = s * dc + c * ds;
    cp.push_back(vec2{x0 + c2, y0 + s2});
    c = c2;
    s = s2;
  }
  cp.push_back(cp[0]);
  if (*a.rotate != 0 && partial) // GlowScript turns only a partial circle
    cp = rotatecp(cp, *a.pos, *a.rotate);
  if (*a.scale != 1)
    a.xscale = a.yscale = *a.scale;
  if (*a.xscale != 1 || *a.yscale != 1)
    cp = scaled(cp, *a.xscale, *a.yscale);
  return {cp};
}

// GlowScript's arc and line ignore scale, xscale and yscale: they compute the scaled points and drop them
inline outline arc(args a)
{
  if (!a.pos)
    a.pos = vec2{0, 0};
  if (!a.radius)
    a.radius = 0.5;
  if (!a.np)
    a.np = npdefault;
  if (!a.thickness)
    a.thickness = 0.01 * *a.radius;
  if (!a.angle1)
    a.angle1 = 0;
  if (!a.angle2)
    a.angle2 = 2 * std::numbers::pi;
  if (!a.rotate)
    a.rotate = 0;
  contour cp;
  contour cpi;
  double seg = 2 * std::numbers::pi / *a.np;
  const int nseg = static_cast<int>(std::floor(std::abs(*a.angle2 - *a.angle1) / seg)) + 1;
  seg = (*a.angle2 - *a.angle1) / nseg;
  for (int i = 0; i < nseg + 1; ++i)
  {
    const double x = std::cos(*a.angle1 + i * seg);
    const double y = std::sin(*a.angle1 + i * seg);
    cp.push_back(vec2{*a.radius * x + a.pos->x(), *a.radius * y + a.pos->y()});
    if (!a.path)
      cpi.push_back(vec2{(*a.radius - *a.thickness) * x + a.pos->x(), (*a.radius - *a.thickness) * y + a.pos->y()});
  }
  if (!a.path)
  {
    std::ranges::reverse(cpi);
    cp.insert(cp.end(), cpi.begin(), cpi.end());
    cp.push_back(cp[0]);
  }
  if (*a.rotate != 0) // GlowScript fails here; this turns it as the other shapes do
    cp = rotatecp(cp, *a.pos, *a.rotate);
  return {cp};
}

inline outline ellipse(args a)
{
  if (!a.width)
    a.width = 1;
  if (!a.height)
    a.height = 0.5 * *a.width;
  if (!a.yscale)
    a.yscale = 1;
  a.yscale = *a.yscale * *a.height / *a.width;
  a.radius = *a.width; // so the ellipse is twice width across, as in GlowScript
  return circle(a);
}

inline outline line(args a)
{
  if (!a.pos)
    a.pos = vec2{0, 0};
  if (!a.np)
    a.np = 2;
  if (!a.rotate)
    a.rotate = 0;
  if (!a.start)
    a.start = vec2{0, 0};
  if (!a.end)
    a.end = vec2{0, 1};
  const vec3 v{a.end->x() - a.start->x(), a.end->y() - a.start->y(), 0};
  if (!a.thickness)
    a.thickness = 0.01 * mag(v);
  const vec3 dv = mesh::detail::gs_norm(vcpp::cross(vec3{0, 0, 1}, v)) * *a.thickness;
  contour cp;
  contour cpi;
  const vec3 vline = v * (1 / std::floor(*a.np - 1));
  for (int i = 0; i < static_cast<int>(*a.np); ++i)
  {
    const double x = a.start->x() + vline.x() * i;
    const double y = a.start->y() + vline.y() * i;
    cp.push_back(vec2{x + a.pos->x(), y + a.pos->y()});
    cpi.push_back(vec2{x + a.pos->x() + dv.x(), y + a.pos->y() + dv.y()});
  }
  std::ranges::reverse(cpi);
  cp.insert(cp.end(), cpi.begin(), cpi.end());
  cp.push_back(cp[0]);
  if (*a.rotate != 0)
    cp = rotatecp(cp, *a.pos, *a.rotate);
  return {cp};
}

inline outline nframe(args a)
{
  common_defaults(a);
  if (!a.length)
    a.length = 1;
  if (!a.np)
    a.np = 3;
  const double t = *a.length * *a.thickness;
  args o;
  o.pos = a.pos;
  o.np = a.np;
  o.length = a.length;
  args i = o;
  const double angle = std::numbers::pi * (0.5 - 1 / *a.np);
  i.length = *a.length - 2 * t / std::tan(angle);
  return frame(ngon(o)[0], ngon(i)[0], a);
}

inline outline ngon(args a)
{
  if (!a.thickness)
    a.thickness = 0;
  common_defaults(a);
  if (!a.length)
    a.length = 1;
  if (!a.np)
    a.np = 3;
  if (*a.np < 3)
    throw std::invalid_argument("vcpp: number of sides can not be less than 3");
  if (*a.thickness != 0)
    return nframe(a);
  const double seg = 2 * std::numbers::pi / *a.np;
  const double radius = *a.length / 2 / std::sin(seg / 2);
  contour cp;
  double angle = 0;
  for (int i = 0; i < static_cast<int>(*a.np); ++i)
  {
    cp.push_back(vec2{radius * std::cos(angle) + a.pos->x(), radius * std::sin(angle) + a.pos->y()});
    angle += seg;
  }
  cp.push_back(cp[0]);
  return {finish(cp, a)};
}

inline outline sframe(args a)
{
  common_defaults(a);
  if (!a.radius)
    a.radius = 1;
  if (!a.n)
    a.n = 5;
  if (!a.iradius)
    a.iradius = 0.5 * *a.radius;
  const double t = *a.thickness * 2 * *a.iradius;
  args o;
  o.pos = a.pos;
  o.n = a.n;
  o.radius = a.radius;
  o.iradius = a.iradius;
  args i = o;
  i.radius = *a.radius - t;
  i.iradius = (*a.radius - t) * *a.iradius / *a.radius;
  return frame(star(o)[0], star(i)[0], a);
}

inline outline star(args a)
{
  common_defaults(a);
  if (!a.radius)
    a.radius = 1;
  if (!a.n)
    a.n = 5;
  if (!a.iradius)
    a.iradius = *a.radius * 0.5;
  if (!a.thickness)
    a.thickness = 0;
  if (*a.thickness != 0)
    return sframe(a);
  contour cp;
  const double dtheta = std::numbers::pi / *a.n;
  double theta = 0;
  for (int i = 0; i < 2 * static_cast<int>(*a.n) + 1; ++i)
  {
    const double r = i % 2 == 0 ? *a.radius : *a.iradius;
    cp.push_back(vec2{-r * std::sin(theta), r * std::cos(theta)});
    theta += dtheta;
  }
  cp = addpos(*a.pos, cp);
  cp.back() = cp.front();
  return {finish(cp, a)};
}

inline outline points(args a)
{
  common_defaults(a);
  contour cp = a.points.value_or(contour{});
  if (cp.empty())
    return {cp};
  if (cp.back() != cp.front())
    cp.push_back(cp.front());
  if (*a.rotate != 0)
    cp = rotatecp(cp, cp[0], *a.rotate);
  a.rotate = 0;
  return {finish(cp, a, false)};
}

// The involute profile of one tooth (GlowScript's port of Stefano Selleri's Blender gear script)
inline contour tooth_outline(double n, double res, double phi, double radius, double addendum, double dedendum,
                             double fradius, double bevel)
{
  const double pi = std::numbers::pi;
  const double rbottom = radius - dedendum - fradius;
  const double rded = radius - dedendum;
  const double rbase = radius * std::cos(phi * pi / 180);
  const double rbevel = radius + addendum - bevel;
  const double radd = radius + addendum;
  const double diametral_pitch = n / (2 * radius);
  const double tooth_thickness = pi / 2 / diametral_pitch;
  const double circular_pitch = pi / diametral_pitch;
  const double u1 = std::sqrt((1 - std::cos(phi * pi / 1800)) / std::cos(phi * pi / 180)); // 1800 as in GlowScript
  const double u2 = std::sqrt(rbevel * rbevel / (rded * rded) - 1);
  const double theta_a1 = std::atan((std::sin(u1) - u1 * std::cos(u1)) / (std::cos(u1) + u1 * std::sin(u1)));
  const double theta_a2 = std::atan((std::sin(u2) - u2 * std::cos(u2)) / (std::cos(u2) + u2 * std::sin(u2)));
  const double theta_a3 = theta_a1 + tooth_thickness / (radius * 2);
  const double theta0 = circular_pitch / (radius * 2);
  const double theta1 = theta_a3 + fradius / rded;
  const double theta2 = theta_a3;
  double theta4 = theta_a3 - theta_a2 - bevel / radd;

  const int N = static_cast<int>(res);
  contour pts;
  for (int i = 0; i < 2 * N; ++i) // bottom of the tooth
  {
    const double th = (theta1 - theta0) * i / (2 * N - 1) + theta0;
    pts.push_back(vec2{rbottom * std::cos(th), rbottom * std::sin(th)});
  }
  double xc = rded * std::cos(theta1); // bottom fillet
  double yc = rded * std::sin(theta1);
  const double aw = pi / 2 + theta2 - theta1;
  for (int i = 0; i < N; ++i)
  {
    const double th = aw * (i + 1) / N + pi + theta1;
    pts.push_back(vec2{xc + fradius * std::cos(th), yc + fradius * std::sin(th)});
  }
  for (int i = 0; i < N; ++i) // straight part
  {
    const double r = (rbase - rded) * (i + 1) / N + rded;
    pts.push_back(vec2{r * std::cos(theta2), r * std::sin(theta2)});
  }
  double u = 0;
  for (int i = 0; i < 3 * N; ++i) // involute
  {
    const double r = (rbevel - rbase) * (i + 1) / (3 * N) + rbase;
    u = std::sqrt(r * r / (rbase * rbase) - 1);
    const double xp = rbase * (std::cos(u) + u * std::sin(u));
    const double yp = -rbase * (std::sin(u) - u * std::cos(u));
    pts.push_back(vec2{xp * std::cos(theta2) - yp * std::sin(theta2), xp * std::sin(theta2) + yp * std::cos(theta2)});
  }
  const double auxth = -u + theta_a3 + pi / 2; // bevel
  vec2 p0 = pts.back();
  const double ra = bevel / (1 - std::cos(auxth - theta4));
  xc = p0.x() - ra * std::cos(auxth);
  yc = p0.y() - ra * std::sin(auxth);
  for (int i = 0; i < N; ++i)
  {
    const double th = (theta4 - auxth) * (i + 1) / N + auxth;
    pts.push_back(vec2{xc + ra * std::cos(th), yc + ra * std::sin(th)});
  }
  p0 = pts.back(); // top
  theta4 = std::atan(p0.y() / p0.x());
  const double rtop = std::sqrt(p0.x() * p0.x() + p0.y() * p0.y());
  for (int i = 0; i < N; ++i)
  {
    const double th = -theta4 * (i + 1) / N + theta4;
    pts.push_back(vec2{rtop * std::cos(th), rtop * std::sin(th)});
  }
  const std::size_t m = pts.size(); // the mirror image
  for (std::size_t i = 0; i + 1 < m; ++i)
  {
    const vec2 p = pts[m - 2 - i];
    pts.push_back(vec2{p.x(), -p.y()});
  }
  return pts;
}

inline contour rack_outline(double n, double res, double phi, double radius, double addendum, double dedendum,
                            double fradius, double bevel)
{
  const double pi = std::numbers::pi;
  const double xbottom = -dedendum - fradius;
  const double xded = -dedendum;
  const double xadd = addendum;
  const double diametral_pitch = n / (2 * radius);
  const double tooth_thickness = pi / 2 / diametral_pitch;
  const double circular_pitch = pi / diametral_pitch;
  const double pa = phi * pi / 180;
  const double ya1 = tooth_thickness / 2;
  const double ya2 = (-xded + fradius * std::sin(pa)) * std::tan(pa);
  const double ya3 = fradius * std::cos(pa);
  const double y0 = circular_pitch / 2;
  const double y1 = ya1 + ya2 + ya3;
  const double y4 = ya1 - (xadd - bevel) * std::tan(pa) - std::cos(pa) / (1 - std::sin(pa)) * bevel;

  const int N = static_cast<int>(res);
  contour pts;
  for (int i = fradius != 0 ? 1 : 0; i < 2 * N; ++i)
    pts.push_back(vec2{xbottom, (y1 - y0) * i / (2 * N - 1) + y0});
  const double aw = pi / 2 - pa; // bottom fillet
  for (int i = 0; i < N; ++i)
  {
    const double th = aw * (i + 1) / N + pi;
    pts.push_back(vec2{xded + fradius * std::cos(th), y1 + fradius * std::sin(th)});
  }
  const double xd = xded - fradius * std::sin(pa); // straight part
  for (int i = 0; i < 4 * N; ++i)
  {
    const double x = (xadd - bevel - xd) * (i + 1) / (4 * N) + xd;
    pts.push_back(vec2{x, ya1 - std::tan(pa) * x});
  }
  const double ra = bevel / (1 - std::sin(pa)); // bevel
  const double xc = xadd - ra;
  for (int i = 0; i < N; ++i)
  {
    const double th = (-pi / 2 + pa) * (i + 1) / N + pi / 2 - pa;
    pts.push_back(vec2{xc + ra * std::cos(th), y4 + ra * std::sin(th)});
  }
  for (int i = 0; i < N; ++i) // top
    pts.push_back(vec2{xadd, -y4 * (i + 1) / N + y4});
  const std::size_t m = pts.size();
  for (std::size_t i = 0; i + 1 < m; ++i)
  {
    const vec2 p = pts[m - 2 - i];
    pts.push_back(vec2{p.x(), -p.y()});
  }
  return pts;
}

inline outline gear(args a)
{
  if (!a.pos)
    a.pos = vec2{0, 0};
  if (!a.n)
    a.n = 20;
  if (!a.radius)
    a.radius = 1;
  if (!a.phi)
    a.phi = 20;
  if (!a.addendum)
    a.addendum = 0.08 * *a.radius;
  if (!a.dedendum)
    a.dedendum = 0.1 * *a.radius;
  if (!a.fradius)
    a.fradius = 0.02 * *a.radius;
  if (!a.rotate)
    a.rotate = 0;
  if (!a.scale)
    a.scale = 1;
  if (!a.res)
    a.res = 1;
  const contour tooth =
    tooth_outline(*a.n, *a.res, *a.phi, *a.radius, *a.addendum, *a.dedendum, *a.fradius, 0); // no bevel
  contour g;
  for (int i = 0; i < static_cast<int>(*a.n); ++i)
  {
    const double rotan = -i * 2 * std::numbers::pi / *a.n;
    for (const vec2& p : tooth)
      g.push_back(vec2{p.x() * std::cos(rotan) - p.y() * std::sin(rotan) + a.pos->x(),
                       p.x() * std::sin(rotan) + p.y() * std::cos(rotan) + a.pos->y()});
  }
  if (*a.scale != 1)
    g = scaled(g, *a.scale, *a.scale);
  if (*a.rotate != 0)
    g = rotatecp(g, *a.pos, *a.rotate);
  // GlowScript's pass to drop repeated points; as written it also keeps only every other point
  contour pts;
  for (std::size_t i = 0; i < g.size(); ++i)
  {
    const vec2 g1 = g[i];
    pts.push_back(g1);
    if (i == g.size() - 1)
      break;
    vec2 g2 = g[i + 1];
    if (i + 2 < g.size())
    {
      const vec2 g3 = g[i + 2];
      if (std::abs(g3.x() - g1.x()) < 0.001 * *a.radius && std::abs(g3.y() - g1.y()) < 0.001 * *a.radius)
        i += 2;
      g2 = g[i];
      if (g1 == g2)
        ++i;
      continue;
    }
    if (g1 == g2)
      ++i;
  }
  if (pts.front() != pts.back())
    pts.push_back(pts.front());
  return {pts};
}

inline outline rackgear(args a)
{
  if (!a.pos)
    a.pos = vec2{0, 0};
  if (!a.n)
    a.n = 30;
  if (!a.radius)
    a.radius = 5;
  if (!a.phi)
    a.phi = 20;
  if (!a.addendum)
    a.addendum = 0.08 * *a.radius;
  if (!a.dedendum)
    a.dedendum = 0.1 * *a.radius;
  if (!a.fradius)
    a.fradius = 0.02 * *a.radius;
  if (!a.rotate)
    a.rotate = 0;
  if (!a.scale)
    a.scale = 1;
  if (!a.length)
    a.length = 10 * std::numbers::pi;
  if (!a.res)
    a.res = 1;
  if (!a.bevel)
    a.bevel = 0.05;
  if (!a.depth)
    a.depth = 0.4 + 0.6 + 0.1;
  const contour tooth = rack_outline(*a.n, *a.res, *a.phi, *a.radius, *a.addendum, *a.dedendum, *a.fradius, *a.bevel);
  const double toothl = tooth.front().y() - tooth.back().y();
  const int nt = static_cast<int>(std::floor(*a.length / toothl));
  contour g;
  double lastx = 10000;
  double lasty = 10000;
  for (int i = 0; i < nt; ++i)
    for (const vec2& p : tooth)
    {
      if (p.x() == lastx || p.y() == lasty)
        continue;
      g.push_back(vec2{p.x() + a.pos->x(), -i * toothl + p.y() + a.pos->y()});
      lastx = p.x();
      lasty = p.y();
    }
  g.push_back(vec2{g.back().x() - *a.depth, g.back().y()});
  g.push_back(vec2{g.front().x() - *a.depth, g.front().y()});
  g.push_back(g.front());
  double left = 1e3;
  double right = -1e3;
  double bottom = 1e3;
  double topy = -1e3;
  for (const vec2& p : g)
  {
    left = std::min(left, p.x());
    right = std::max(right, p.x());
    bottom = std::min(bottom, p.y());
    topy = std::max(topy, p.y());
  }
  const double dx = a.pos->x() - (left + right) / 2;
  const double dy = a.pos->y() - (bottom + topy) / 2;
  contour g2;
  for (const vec2& p : g)
    g2.push_back(vec2{p.x() + dx, p.y() + dy});
  if (*a.scale != 1)
    g2 = scaled(g2, *a.scale, *a.scale);
  if (*a.rotate != 0) // GlowScript fails here; this turns it as the other shapes do
    g2 = rotatecp(g2, *a.pos, *a.rotate);
  if (g2.front() != g2.back())
    g2.push_back(g2.front());
  return {g2};
}
} // namespace detail

// GlowScript's shapes. Each returns the outline's contours: the outline, then a hole if thickness made one.
template<typename... Binders>
outline rectangle(const Binders&... binders)
{ return detail::rectangle(detail::read(binders...)); }

template<typename... Binders>
outline cross(const Binders&... binders)
{ return detail::cross(detail::read(binders...)); }

template<typename... Binders>
outline trapezoid(const Binders&... binders)
{ return detail::trapezoid(detail::read(binders...)); }

template<typename... Binders>
outline circle(const Binders&... binders)
{ return detail::circle(detail::read(binders...)); }

template<typename... Binders>
outline arc(const Binders&... binders)
{ return detail::arc(detail::read(binders...)); }

template<typename... Binders>
outline ellipse(const Binders&... binders)
{ return detail::ellipse(detail::read(binders...)); }

template<typename... Binders>
outline line(const Binders&... binders)
{ return detail::line(detail::read(binders...)); }

template<typename... Binders>
outline ngon(const Binders&... binders)
{ return detail::ngon(detail::read(binders...)); }

template<typename... Binders>
outline star(const Binders&... binders)
{ return detail::star(detail::read(binders...)); }

template<typename... Binders>
outline points(const Binders&... binders)
{ return detail::points(detail::read(binders...)); }

template<typename... Binders>
outline gear(const Binders&... binders)
{ return detail::gear(detail::read(binders...)); }

template<typename... Binders>
outline rackgear(const Binders&... binders)
{ return detail::rackgear(detail::read(binders...)); }

template<typename... Binders>
outline triangle(const Binders&... binders)
{
  detail::args a = detail::read(binders...);
  a.np = 3;
  a.rotate = a.rotate.value_or(0) - std::numbers::pi / 6;
  return detail::ngon(a);
}

template<typename... Binders>
outline pentagon(const Binders&... binders)
{
  detail::args a = detail::read(binders...);
  a.np = 5;
  a.rotate = a.rotate.value_or(0) + std::numbers::pi / 10;
  return detail::ngon(a);
}

template<typename... Binders>
outline hexagon(const Binders&... binders)
{
  detail::args a = detail::read(binders...);
  a.np = 6;
  return detail::ngon(a);
}

template<typename... Binders>
outline octagon(const Binders&... binders)
{
  detail::args a = detail::read(binders...);
  a.np = 8;
  a.rotate = a.rotate.value_or(0) + std::numbers::pi / 8;
  return detail::ngon(a);
}

} // namespace vcpp::shapes

export namespace vcpp::paths
{

using namespace vcpp::shapes; // the same named parameters

namespace detail
{
// A shape's outline laid in the xz plane (x stays x, y becomes -z), turned so y points along up, moved to pos
inline std::vector<vec3> convert(const vec3& pos, const vec3& up_in, const shapes::contour& pts)
{
  const vec3 up = mesh::detail::gs_norm(up_in);
  const vec3 up0{0, 1, 0};
  const double angle = std::acos(dot(up, up0));
  const vec3 axis = vcpp::cross(up0, up);
  std::vector<vec3> p;
  for (const vec2& pt : pts)
  {
    vec3 newpt{pt.x(), 0, -pt.y()};
    if (angle > 0)
      newpt = mesh::detail::gs_rotate(newpt, angle, axis);
    p.push_back(pos + newpt);
  }
  return p;
}

inline std::vector<vec3> path_of(shapes::detail::args a, const char* name,
                                 shapes::outline (*shape)(shapes::detail::args), bool thickness_allowed = false)
{
  if (a.thickness && !thickness_allowed)
    throw std::invalid_argument(std::format("vcpp: thickness is not allowed in a {} path", name));
  a.path = true;
  const vec3 pos = a.path_pos.value_or(vec3{0, 0, 0});
  const vec3 up = a.up.value_or(vec3{0, 1, 0});
  return convert(pos, up, shape(a)[0]);
}
} // namespace detail

// GlowScript's paths: a shape's outline as a path in the xz plane, or a straight line
template<typename... Binders>
std::vector<vec3> rectangle(const Binders&... binders)
{ return detail::path_of(shapes::detail::read(binders...), "rectangle", shapes::detail::rectangle); }

template<typename... Binders>
std::vector<vec3> cross(const Binders&... binders)
{ return detail::path_of(shapes::detail::read(binders...), "cross", shapes::detail::cross, true); }

template<typename... Binders>
std::vector<vec3> trapezoid(const Binders&... binders)
{ return detail::path_of(shapes::detail::read(binders...), "trapezoid", shapes::detail::trapezoid); }

template<typename... Binders>
std::vector<vec3> circle(const Binders&... binders)
{ return detail::path_of(shapes::detail::read(binders...), "circle", shapes::detail::circle); }

template<typename... Binders>
std::vector<vec3> arc(const Binders&... binders)
{ return detail::path_of(shapes::detail::read(binders...), "arc", shapes::detail::arc); }

template<typename... Binders>
std::vector<vec3> ellipse(const Binders&... binders)
{ return detail::path_of(shapes::detail::read(binders...), "ellipse", shapes::detail::ellipse); }

template<typename... Binders>
std::vector<vec3> ngon(const Binders&... binders)
{ return detail::path_of(shapes::detail::read(binders...), "ngon", shapes::detail::ngon); }

template<typename... Binders>
std::vector<vec3> star(const Binders&... binders)
{ return detail::path_of(shapes::detail::read(binders...), "star", shapes::detail::star); }

template<typename... Binders>
std::vector<vec3> triangle(const Binders&... binders)
{
  shapes::detail::args a = shapes::detail::read(binders...);
  return detail::path_of(a, "triangle", [](shapes::detail::args b) {
    b.np = 3;
    b.rotate = b.rotate.value_or(0) - std::numbers::pi / 6;
    return shapes::detail::ngon(b);
  });
}

template<typename... Binders>
std::vector<vec3> pentagon(const Binders&... binders)
{
  shapes::detail::args a = shapes::detail::read(binders...);
  return detail::path_of(a, "pentagon", [](shapes::detail::args b) {
    b.np = 5;
    b.rotate = b.rotate.value_or(0) + std::numbers::pi / 10;
    return shapes::detail::ngon(b);
  });
}

template<typename... Binders>
std::vector<vec3> hexagon(const Binders&... binders)
{
  shapes::detail::args a = shapes::detail::read(binders...);
  return detail::path_of(a, "hexagon", [](shapes::detail::args b) {
    b.np = 6;
    return shapes::detail::ngon(b);
  });
}

template<typename... Binders>
std::vector<vec3> octagon(const Binders&... binders)
{
  shapes::detail::args a = shapes::detail::read(binders...);
  return detail::path_of(a, "octagon", [](shapes::detail::args b) {
    b.np = 8;
    b.rotate = b.rotate.value_or(0) + std::numbers::pi / 8;
    return shapes::detail::ngon(b);
  });
}

// np points from start to end
template<typename... Binders>
std::vector<vec3> line(const Binders&... binders)
{
  shapes::detail::args a = shapes::detail::read(binders...);
  if (a.thickness)
    throw std::invalid_argument("vcpp: thickness is not allowed in a line path");
  const double np = a.np.value_or(2);
  const vec3 s = a.path_start.value_or(vec3{0, 0, 0});
  const vec3 e = a.path_end.value_or(vec3{0, 0, -0.1});
  const vec3 dv = (e - s) * (1 / std::floor(np - 1));
  std::vector<vec3> cp{s};
  for (int i = 1; i < static_cast<int>(np); ++i)
    cp.push_back(cp.back() + dv);
  return cp;
}

} // namespace vcpp::paths
