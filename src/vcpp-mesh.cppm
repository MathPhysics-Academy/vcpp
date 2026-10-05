/*
 *  vcpp:mesh - Shared mesh generation utilities
 *
 *  Provides:
 *  - Unified vertex format with UVs
 *  - Mesh generators: sphere, box, cylinder, cone, helix
 *  - Tube mesh generation for curves
 *
 *  Extracted from backends for code deduplication.
 */

module;

import std;

export module vcpp:mesh;

import :vec;

export namespace vcpp::mesh
{

// ============================================================================
// Vertex Format - Unified across all backends
//
// Includes UV coordinates for texture mapping.
// ============================================================================

struct vertex
{
  float position[3];
  float normal[3];
  float uv[2];
};

// ============================================================================
// Mesh Data Structure
// ============================================================================

struct mesh_data
{
  std::vector<vertex> vertices;
  std::vector<std::uint32_t> indices;

  bool valid() const noexcept { return !vertices.empty() && !indices.empty(); }
  std::size_t vertex_count() const noexcept { return vertices.size(); }
  std::size_t index_count() const noexcept { return indices.size(); }
  std::size_t triangle_count() const noexcept { return indices.size() / 3; }
};

// ============================================================================
// Constants
// ============================================================================

inline constexpr float PI = 3.14159265358979323846f;
inline constexpr float TWO_PI = 2.0f * PI;

// ============================================================================
// Sphere, box, cylinder, cone and pyramid: GlowScript's meshes (mesh.js), with its texture coordinates,
// in which v = 1 is the top of the image
// ============================================================================

inline vertex make_vertex(double px, double py, double pz, double nx, double ny, double nz, double u, double v)
{
  return {{static_cast<float>(px), static_cast<float>(py), static_cast<float>(pz)},
          {static_cast<float>(nx), static_cast<float>(ny), static_cast<float>(nz)},
          {static_cast<float>(u), static_cast<float>(v)}};
}

// Radius 0.5, poles on y. Latitudes run from the north pole, each starting at -z and turning toward -x.
// The image wraps once around: u = 0 at -z, v = 1 at the north pole.
inline mesh_data generate_sphere(int n = 30)
{
  mesh_data mesh;
  const double r = 0.5;
  const int offset = n + 1;
  const double sint = std::sin(std::numbers::pi / n);
  const double cost = std::cos(std::numbers::pi / n);
  const double sinp = std::sin(2 * std::numbers::pi / n);
  const double cosp = std::cos(2 * std::numbers::pi / n);
  double y1 = r;
  double z1 = 0;
  for (int i = 0; i <= n; ++i)
  {
    const double y2 = y1 * cost + z1 * sint;
    const double z2 = z1 * cost - y1 * sint;
    double x = 0;
    double z = z1;
    for (int j = 0; j < offset; ++j)
    {
      mesh.vertices.push_back(
        make_vertex(x, y1, z, x / r, y1 / r, z / r, static_cast<double>(j) / n, 1 - static_cast<double>(i) / n));
      const double nx = x * cosp + z * sinp;
      z = z * cosp - x * sinp;
      x = nx;
    }
    y1 = y2;
    z1 = z2;
  }
  for (int i = 0; i < n; ++i)
    for (int j = 0; j < n; ++j)
    {
      const auto s = static_cast<std::uint32_t>(i * offset + j);
      const auto o = static_cast<std::uint32_t>(offset);
      mesh.indices.insert(mesh.indices.end(), {s, s + o, s + o + 1, s, s + o + 1, s + 1});
    }
  return mesh;
}

// A 1x1x1 cube centred on the origin; each face shows the whole image
inline mesh_data generate_box()
{
  mesh_data mesh;
  const double s = 0.5;
  const double pos[24][3] = {
    {+s, +s, +s}, {+s, -s, +s}, {+s, -s, -s}, {+s, +s, -s}, // right
    {-s, +s, -s}, {-s, -s, -s}, {-s, -s, +s}, {-s, +s, +s}, // left
    {-s, -s, +s}, {-s, -s, -s}, {+s, -s, -s}, {+s, -s, +s}, // bottom
    {-s, +s, -s}, {-s, +s, +s}, {+s, +s, +s}, {+s, +s, -s}, // top
    {-s, +s, +s}, {-s, -s, +s}, {+s, -s, +s}, {+s, +s, +s}, // front
    {+s, +s, -s}, {+s, -s, -s}, {-s, -s, -s}, {-s, +s, -s}, // back
  };
  const double normals[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, -1, 0}, {0, 1, 0}, {0, 0, 1}, {0, 0, -1}};
  const double tex[4][2] = {{0, 1}, {0, 0}, {1, 0}, {1, 1}};
  for (std::uint32_t f = 0; f < 6; ++f)
  {
    for (std::uint32_t k = 0; k < 4; ++k)
    {
      const auto& p = pos[f * 4 + k];
      const auto& nm = normals[f];
      mesh.vertices.push_back(make_vertex(p[0], p[1], p[2], nm[0], nm[1], nm[2], tex[k][0], tex[k][1]));
    }
    const std::uint32_t b = f * 4;
    mesh.indices.insert(mesh.indices.end(), {b, b + 1, b + 2, b, b + 2, b + 3});
  }
  return mesh;
}

// Along x from 0 to 1, radius 0.5. Around the side the image runs along the axis (u) and is mirrored about
// the halfway point around (v goes 0 to 1 and back); each end shows it whole.
inline mesh_data generate_cylinder(int sides = 50)
{
  mesh_data mesh;
  const double r = 0.5;
  const int k = 4 * sides;
  const double sind = std::sin(2 * std::numbers::pi / sides);
  const double cosd = std::cos(2 * std::numbers::pi / sides);
  mesh.vertices.push_back(make_vertex(0, 0, 0, -1, 0, 0, 0.5, 0.5));
  mesh.vertices.push_back(make_vertex(1, 0, 0, 1, 0, 0, 0.5, 0.5));
  double y = -r;
  double z = 0;
  for (int i = 2; i <= 2 + k; i += 4)
  {
    const double around =
      i < 2 + 2 * sides ? (i - 2) / 4.0 / (sides / 2.0) : 1 - (i - 2 - 2 * sides) / 4.0 / (sides / 2.0);
    mesh.vertices.push_back(make_vertex(0, y, z, -1, 0, 0, 0.5 * (1 + z / r), 0.5 + 0.5 * y / r));
    mesh.vertices.push_back(make_vertex(0, y, z, 0, y, z, 0, around));
    mesh.vertices.push_back(make_vertex(1, y, z, 1, 0, 0, 0.5 * (1 - z / r), 0.5 + 0.5 * y / r));
    mesh.vertices.push_back(make_vertex(1, y, z, 0, y, z, 1, around));
    if (i != 2 + k)
    {
      auto at = [&](int j) { return static_cast<std::uint32_t>((j - 2) % k + 2); };
      const auto u = static_cast<std::uint32_t>(i);
      mesh.indices.insert(mesh.indices.end(), {0, at(i), at(i + 4), u + 1, at(i + 3), at(i + 7), u + 1, at(i + 7),
                                               at(i + 5), 1, at(i + 6), at(i + 2)});
    }
    const double ny = y * cosd + z * sind;
    z = z * cosd - y * sind;
    y = ny;
  }
  return mesh;
}

// Base at x = 0, radius 0.5, tip at x = 1. Around the side u goes from 1 to 0 and v from base to tip; the
// base shows the image whole.
inline mesh_data generate_cone(int sides = 200)
{
  mesh_data mesh;
  const double r = 0.5;
  const double kn = 1 / (r * std::sqrt(5.0));
  const double sind = std::sin(2 * std::numbers::pi / sides);
  const double cosd = std::cos(2 * std::numbers::pi / sides);
  mesh.vertices.push_back(make_vertex(0, 0, 0, -1, 0, 0, 0.5, 0.5));
  double y = 0;
  double z = -r;
  for (int i = 1; i <= 1 + 3 * sides; i += 3)
  {
    const double ny = y * cosd + z * sind;
    const double nz = z * cosd - y * sind;
    const double u = 1 - (i - 1) / 3.0 / sides;
    mesh.vertices.push_back(make_vertex(0, y, z, -1, 0, 0, 0.5 * (1 + z / r), 0.5 * (1 + y / r)));
    mesh.vertices.push_back(make_vertex(0, y, z, kn * r, 2 * kn * y, 2 * kn * z, u, 0));
    mesh.vertices.push_back(make_vertex(1, 0, 0, kn * r, kn * (y + ny), kn * (z + nz), u, 1));
    if (i != 1 + 3 * sides)
    {
      const auto v = static_cast<std::uint32_t>(i);
      mesh.indices.insert(mesh.indices.end(), {0, v, v + 3, v + 1, v + 2, v + 4});
    }
    y = ny;
    z = nz;
  }
  return mesh;
}

// Helix Mesh Generator
//
// A coiled tube along X from 0 to length, as GlowScript draws a helix: 60 steps per coil, starting at +z
// and turning toward +y when ccw.
// ============================================================================

inline mesh_data generate_helix(float length = 1.0f, float helix_radius = 1.0f, float tube_radius = 0.1f, int coils = 8,
                                bool ccw = true, int tube_slices = 8, int steps_per_coil = 60)
{
  mesh_data mesh;
  const float turn = ccw ? 1.0f : -1.0f;
  int total_steps = coils * steps_per_coil;

  for (int i = 0; i <= total_steps; ++i)
  {
    float t = static_cast<float>(i) / static_cast<float>(total_steps);
    float angle = turn * t * static_cast<float>(coils) * TWO_PI;
    float x_center = t * length;

    // Helix path point
    float y_center = helix_radius * std::sin(angle);
    float z_center = helix_radius * std::cos(angle);

    // Tangent vector
    float A = turn * static_cast<float>(coils) * TWO_PI;
    float tx = length;
    float ty = helix_radius * A * std::cos(angle);
    float tz = -helix_radius * A * std::sin(angle);
    float t_len = std::sqrt(tx * tx + ty * ty + tz * tz);
    tx /= t_len;
    ty /= t_len;
    tz /= t_len;

    // Normal (radial direction)
    float nx = 0;
    float ny = std::sin(angle);
    float nz = std::cos(angle);

    // Binormal B = T x N
    float bx = ty * nz - tz * ny;
    float by = tz * nx - tx * nz;
    float bz = tx * ny - ty * nx;

    // Generate ring of vertices for tube
    for (int j = 0; j <= tube_slices; ++j)
    {
      float phi = TWO_PI * static_cast<float>(j) / static_cast<float>(tube_slices);
      float cos_phi = std::cos(phi);
      float sin_phi = std::sin(phi);

      // Tube offset in N-B plane
      float off_x = tube_radius * (cos_phi * nx + sin_phi * bx);
      float off_y = tube_radius * (cos_phi * ny + sin_phi * by);
      float off_z = tube_radius * (cos_phi * nz + sin_phi * bz);

      float px = x_center + off_x;
      float py = y_center + off_y;
      float pz = z_center + off_z;

      // Surface normal
      float n_len = std::sqrt(off_x * off_x + off_y * off_y + off_z * off_z);
      float snx = off_x / n_len;
      float sny = off_y / n_len;
      float snz = off_z / n_len;

      vertex v{};
      v.position[0] = px;
      v.position[1] = py;
      v.position[2] = pz;
      v.normal[0] = snx;
      v.normal[1] = sny;
      v.normal[2] = snz;
      v.uv[0] = t;
      v.uv[1] = static_cast<float>(j) / static_cast<float>(tube_slices);
      mesh.vertices.push_back(v);
    }
  }

  // Indices
  for (int i = 0; i < total_steps; ++i)
  {
    for (int j = 0; j < tube_slices; ++j)
    {
      int current_ring = i * (tube_slices + 1);
      int next_ring = (i + 1) * (tube_slices + 1);
      std::uint32_t v0 = static_cast<std::uint32_t>(current_ring + j);
      std::uint32_t v1 = static_cast<std::uint32_t>(next_ring + j);
      std::uint32_t v2 = static_cast<std::uint32_t>(current_ring + j + 1);
      std::uint32_t v3 = static_cast<std::uint32_t>(next_ring + j + 1);

      mesh.indices.push_back(v0);
      mesh.indices.push_back(v1);
      mesh.indices.push_back(v2);

      mesh.indices.push_back(v1);
      mesh.indices.push_back(v3);
      mesh.indices.push_back(v2);
    }
  }

  return mesh;
}

// ============================================================================
// Ring/Torus Mesh Generator
//
// Major radius 1.0, minor radius (thickness) 0.1.
// Ring axis along X.
// ============================================================================

inline mesh_data generate_ring(float major_radius = 1.0f, float minor_radius = 0.1f, int major_segments = 32,
                               int minor_segments = 12)
{
  mesh_data mesh;

  for (int i = 0; i <= major_segments; ++i)
  {
    float u = static_cast<float>(i) / static_cast<float>(major_segments);
    float theta = u * TWO_PI;
    float cos_theta = std::cos(theta);
    float sin_theta = std::sin(theta);

    // Center of the tube cross-section
    float cx = 0.0f; // Ring plane is YZ
    float cy = major_radius * cos_theta;
    float cz = major_radius * sin_theta;

    for (int j = 0; j <= minor_segments; ++j)
    {
      float v = static_cast<float>(j) / static_cast<float>(minor_segments);
      float phi = v * TWO_PI;
      float cos_phi = std::cos(phi);
      float sin_phi = std::sin(phi);

      // Position on the tube surface
      // Offset in the radial direction (toward center) and X direction
      float px = minor_radius * sin_phi;
      float py = cy + minor_radius * cos_phi * cos_theta;
      float pz = cz + minor_radius * cos_phi * sin_theta;

      // Normal
      float nx = sin_phi;
      float ny = cos_phi * cos_theta;
      float nz = cos_phi * sin_theta;

      vertex vert{};
      vert.position[0] = px;
      vert.position[1] = py;
      vert.position[2] = pz;
      vert.normal[0] = nx;
      vert.normal[1] = ny;
      vert.normal[2] = nz;
      vert.uv[0] = u;
      vert.uv[1] = v;
      mesh.vertices.push_back(vert);
    }
  }

  // Indices
  for (int i = 0; i < major_segments; ++i)
  {
    for (int j = 0; j < minor_segments; ++j)
    {
      int current = i * (minor_segments + 1) + j;
      int next = (i + 1) * (minor_segments + 1) + j;

      mesh.indices.push_back(static_cast<std::uint32_t>(current));
      mesh.indices.push_back(static_cast<std::uint32_t>(next));
      mesh.indices.push_back(static_cast<std::uint32_t>(current + 1));

      mesh.indices.push_back(static_cast<std::uint32_t>(current + 1));
      mesh.indices.push_back(static_cast<std::uint32_t>(next));
      mesh.indices.push_back(static_cast<std::uint32_t>(next + 1));
    }
  }

  return mesh;
}

// ============================================================================
// Tubes along a polyline (curves and trails)
//
// Each point gets a ring of tube_slices + 1 vertices (the last repeats the first, for the texture seam).
// A ring's orientation is carried from the ring before it (parallel transport), so the tube doesn't twist.
// ============================================================================

// The direction of a polyline at point i: along the neighbouring points
inline vec3 tube_tangent(std::span<const vec3> points, std::size_t i)
{
  const std::size_t n = points.size();
  const vec3 t = i == 0       ? points[1] - points[0]
                 : i == n - 1 ? points[n - 1] - points[n - 2]
                              : points[i + 1] - points[i - 1];
  return mag2(t) < 1e-12 ? vec3{1, 0, 0} : hat(t);
}

// The ring orientation at the first point
inline vec3 tube_first_normal(const vec3& tangent)
{
  const vec3 guide = std::abs(dot(tangent, vec3{0, 1, 0})) > 0.99 ? vec3{0, 0, 1} : vec3{0, 1, 0};
  return hat(cross(tangent, guide));
}

// The previous ring's orientation carried onto a point with this tangent
inline vec3 tube_next_normal(const vec3& previous, const vec3& tangent)
{
  vec3 proj = previous - tangent * dot(previous, tangent);
  if (mag2(proj) < 1e-12)
    proj = cross(tangent, std::abs(dot(tangent, vec3{0, 1, 0})) > 0.99 ? vec3{0, 0, 1} : vec3{0, 1, 0});
  return hat(proj);
}

// Writes the ring around `center` into out (tube_slices + 1 vertices); v is the ring's texture coordinate
inline void tube_ring(std::span<vertex> out, const vec3& center, const vec3& tangent, const vec3& normal, float radius,
                      float v)
{
  const int slices = static_cast<int>(out.size()) - 1;
  const vec3 binormal = cross(tangent, normal);
  for (int j = 0; j <= slices; ++j)
  {
    const double phi = TWO_PI * static_cast<double>(j) / static_cast<double>(slices);
    const vec3 offset = normal * std::cos(phi) + binormal * std::sin(phi);
    const vec3 pos = center + offset * static_cast<double>(radius);
    vertex& out_v = out[static_cast<std::size_t>(j)];
    out_v = vertex{};
    out_v.position[0] = static_cast<float>(pos.x());
    out_v.position[1] = static_cast<float>(pos.y());
    out_v.position[2] = static_cast<float>(pos.z());
    out_v.normal[0] = static_cast<float>(offset.x());
    out_v.normal[1] = static_cast<float>(offset.y());
    out_v.normal[2] = static_cast<float>(offset.z());
    out_v.uv[0] = v;
    out_v.uv[1] = static_cast<float>(j) / static_cast<float>(slices);
  }
}

// The two triangles of each side between ring a and ring b, as indices (rings of tube_slices + 1 vertices)
inline void tube_segment(std::vector<std::uint32_t>& out, std::uint32_t ring_a, std::uint32_t ring_b, int tube_slices)
{
  const auto stride = static_cast<std::uint32_t>(tube_slices + 1);
  for (std::uint32_t j = 0; j < static_cast<std::uint32_t>(tube_slices); ++j)
  {
    const std::uint32_t v0 = ring_a * stride + j;
    const std::uint32_t v1 = ring_b * stride + j;
    out.insert(out.end(), {v0, v1, v0 + 1, v1, v1 + 1, v0 + 1});
  }
}

inline mesh_data generate_tube(const std::vector<vec3>& points, float radius, int tube_slices = 8)
{
  mesh_data mesh;
  if (points.size() < 2)
    return mesh;

  const std::size_t n = points.size();
  const auto stride = static_cast<std::size_t>(tube_slices + 1);
  mesh.vertices.resize(n * stride);
  vec3 normal;
  for (std::size_t i = 0; i < n; ++i)
  {
    const vec3 tangent = tube_tangent(points, i);
    normal = i == 0 ? tube_first_normal(tangent) : tube_next_normal(normal, tangent);
    tube_ring(std::span{mesh.vertices}.subspan(i * stride, stride), points[i], tangent, normal, radius,
              static_cast<float>(i) / static_cast<float>(n - 1));
  }
  for (std::size_t i = 0; i + 1 < n; ++i)
    tube_segment(mesh.indices, static_cast<std::uint32_t>(i), static_cast<std::uint32_t>(i + 1), tube_slices);
  return mesh;
}

// ============================================================================
// tube_builder - A trail's tube, updated one point at a time
//
// Holds the tube's vertices in buffer slots, one ring per slot. Each update writes only the newest rings
// and the one before them, whose direction changes once a point follows it. With retain, the slots form a
// ring buffer of `retain` rings and the tube is drawn as one or two index ranges; without it the buffer
// grows by doubling. The whole tube is rebuilt only on the first update, when retain or the radius changes,
// when the trail restarts, or when the buffer must grow.
// ============================================================================

class tube_builder
{
public:
  explicit tube_builder(int tube_slices = 6) : m_slices(tube_slices) {}

  // Brings the tube up to date with points (newest last); `added` counts every point the trail has ever
  // added. True when every vertex and index changed; otherwise changed_rings() lists the slots rewritten.
  bool update(std::span<const vec3> points, std::uint64_t added, int retain, float radius)
  {
    m_changed.clear();
    const std::size_t n = points.size();
    const bool bounded = retain > 0;
    const std::uint64_t fresh = added - m_seen;
    if (n < 2)
    {
      m_rings = n;
      m_seen = added;
      return false;
    }
    if (m_capacity == 0 || added < m_seen || fresh >= n || retain != m_retain || radius != m_radius ||
        (bounded ? m_capacity != static_cast<std::size_t>(retain) : n > m_capacity))
    {
      rebuild(points, retain, radius);
      m_seen = added;
      return true;
    }
    if (fresh == 0)
      return false;

    const std::size_t dropped = m_rings + fresh - n; // pushed out by retain
    m_first = bounded ? (m_first + dropped) % m_capacity : 0;
    vec3 normal = m_last_normal;
    for (std::size_t i = n - fresh - 1; i < n; ++i)
    {
      const vec3 tangent = tube_tangent(points, i);
      normal = tube_next_normal(normal, tangent);
      write_ring(slot(i), points[i], tangent, normal);
    }
    m_last_normal = normal;
    m_rings = n;
    m_seen = added;
    return false;
  }

  std::span<const vertex> vertices() const { return m_vertices; }
  std::span<const std::uint32_t> indices() const { return m_indices; }
  const std::vector<std::size_t>& changed_rings() const { return m_changed; }
  std::size_t ring_size() const { return static_cast<std::size_t>(m_slices) + 1; }

  // The index ranges to draw, as (first index, count): two when the ring buffer wraps, else one and an empty one
  std::array<std::pair<std::uint32_t, std::uint32_t>, 2> draw_ranges() const
  {
    if (m_rings < 2)
      return {};
    const std::size_t per_segment = 6 * static_cast<std::size_t>(m_slices);
    const std::size_t segments = m_rings - 1;
    const std::size_t first_part = std::min(segments, m_capacity - m_first);
    return {{{static_cast<std::uint32_t>(m_first * per_segment), static_cast<std::uint32_t>(first_part * per_segment)},
             {0, static_cast<std::uint32_t>((segments - first_part) * per_segment)}}};
  }

private:
  int m_slices;
  std::vector<vertex> m_vertices;
  std::vector<std::uint32_t> m_indices;
  std::vector<std::size_t> m_changed;
  std::size_t m_capacity{0}; // ring slots
  std::size_t m_first{0};    // slot of the oldest ring
  std::size_t m_rings{0};
  std::uint64_t m_seen{0};
  int m_retain{-1};
  float m_radius{0};
  vec3 m_last_normal;

  std::size_t slot(std::size_t i) const { return m_retain > 0 ? (m_first + i) % m_capacity : i; }

  void write_ring(std::size_t at, const vec3& center, const vec3& tangent, const vec3& normal)
  {
    tube_ring(std::span{m_vertices}.subspan(at * ring_size(), ring_size()), center, tangent, normal, m_radius, 0);
    m_changed.push_back(at);
  }

  void rebuild(std::span<const vec3> points, int retain, float radius)
  {
    const std::size_t n = points.size();
    m_retain = retain;
    m_radius = radius;
    m_capacity = retain > 0 ? static_cast<std::size_t>(retain) : std::max<std::size_t>(64, 2 * n);
    m_first = 0;
    m_vertices.assign(m_capacity * ring_size(), vertex{});
    m_indices.clear();
    // Segment s joins slot s to the next slot; with retain the last one wraps to slot 0
    const std::size_t segments = retain > 0 ? m_capacity : m_capacity - 1;
    for (std::size_t s = 0; s < segments; ++s)
      tube_segment(m_indices, static_cast<std::uint32_t>(s), static_cast<std::uint32_t>((s + 1) % m_capacity),
                   m_slices);
    vec3 normal;
    for (std::size_t i = 0; i < n; ++i)
    {
      const vec3 tangent = tube_tangent(points, i);
      normal = i == 0 ? tube_first_normal(tangent) : tube_next_normal(normal, tangent);
      write_ring(i, points[i], tangent, normal);
    }
    m_changed.clear();
    m_last_normal = normal;
    m_rings = n;
  }
};

// ============================================================================
// ============================================================================
// Pyramid Mesh Generator
//
// GlowScript's pyramid: base 1x1 at x = 0, apex at x = 1. The base shows the whole image; the four sides
// share it, a quarter each.
// ============================================================================

inline mesh_data generate_pyramid()
{
  mesh_data mesh;
  const double k = 1 / std::sqrt(5.0);
  const double pos[16][3] = {
    {0, .5, .5},   {0, .5, -.5},  {0, -.5, -.5}, {0, -.5, .5}, // base
    {0, .5, -.5},  {0, .5, .5},   {1, 0, 0},                   // top
    {0, -.5, -.5}, {0, .5, -.5},  {1, 0, 0},                   // back
    {0, -.5, .5},  {0, -.5, -.5}, {1, 0, 0},                   // bottom
    {0, .5, .5},   {0, -.5, .5},  {1, 0, 0},                   // front
  };
  const double normals[16][3] = {
    {-1, 0, 0},     {-1, 0, 0},     {-1, 0, 0},     {-1, 0, 0},     {k, 2 * k, 0},  {k, 2 * k, 0},
    {k, 2 * k, 0},  {k, 0, -2 * k}, {k, 0, -2 * k}, {k, 0, -2 * k}, {k, -2 * k, 0}, {k, -2 * k, 0},
    {k, -2 * k, 0}, {k, 0, 2 * k},  {k, 0, 2 * k},  {k, 0, 2 * k},
  };
  const double tex[16][2] = {
    {1, 1},    {0, 1},    {0, 0},     {1, 0}, // base
    {0, 0},    {0.25, 0}, {0.125, 1},         // top
    {1, 0},    {0.75, 0}, {0.875, 1},         // back
    {0.5, 0},  {0.75, 0}, {0.625, 1},         // bottom
    {0.25, 0}, {0.5, 0},  {0.375, 1},         // front
  };
  for (int i = 0; i < 16; ++i)
    mesh.vertices.push_back(
      make_vertex(pos[i][0], pos[i][1], pos[i][2], normals[i][0], normals[i][1], normals[i][2], tex[i][0], tex[i][1]));
  mesh.indices = {0, 1, 2, 0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
  return mesh;
}
// ============================================================================
// Merge Meshes - Combine multiple meshes into one
//
// Used for compound objects. Concatenates vertices and adjusts indices.
// ============================================================================

// ============================================================================
// triangulate - Fill closed contours, treating a contour inside an odd number of others as a hole
//
// Returns triangles (counter-clockwise in x/y) as indices into the contours' points, numbered in order
// across all contours. Holes are joined to their outer contour by a bridge edge, then the polygon is cut
// into triangles by ear clipping.
// ============================================================================

namespace detail
{
inline double cross2(const vec2& o, const vec2& a, const vec2& b)
{ return (a.x() - o.x()) * (b.y() - o.y()) - (a.y() - o.y()) * (b.x() - o.x()); }

inline double signed_area(const std::vector<vec2>& c)
{
  double area = 0;
  for (std::size_t i = 0, j = c.size() - 1; i < c.size(); j = i++)
    area += (c[j].x() - c[i].x()) * (c[j].y() + c[i].y());
  return area / 2;
}

inline bool inside(const std::vector<vec2>& c, const vec2& p)
{
  bool in = false;
  for (std::size_t i = 0, j = c.size() - 1; i < c.size(); j = i++)
    if ((c[i].y() > p.y()) != (c[j].y() > p.y()) &&
        p.x() < (c[j].x() - c[i].x()) * (p.y() - c[i].y()) / (c[j].y() - c[i].y()) + c[i].x())
      in = !in;
  return in;
}

inline bool in_triangle(const vec2& a, const vec2& b, const vec2& c, const vec2& p)
{ return cross2(a, b, p) >= 0 && cross2(b, c, p) >= 0 && cross2(c, a, p) >= 0; }
} // namespace detail

inline std::vector<std::uint32_t> triangulate(const std::vector<std::vector<vec2>>& contours)
{
  using detail::cross2;
  std::vector<vec2> pts;
  std::vector<std::vector<std::uint32_t>> rings;
  for (const auto& c : contours)
  {
    std::vector<std::uint32_t> ring;
    for (const auto& p : c)
    {
      const auto id = static_cast<std::uint32_t>(pts.size());
      pts.push_back(p);
      if (ring.empty() || pts[ring.back()] != p)
        ring.push_back(id);
    }
    while (ring.size() > 1 && pts[ring.back()] == pts[ring.front()])
      ring.pop_back();
    rings.push_back(std::move(ring));
  }
  auto points_of = [&](const std::vector<std::uint32_t>& ring) {
    std::vector<vec2> out;
    for (auto i : ring)
      out.push_back(pts[i]);
    return out;
  };

  const std::size_t n = rings.size();
  std::vector<int> depth(n, 0);
  for (std::size_t i = 0; i < n; ++i)
    for (std::size_t j = 0; j < n; ++j)
      if (i != j && rings[i].size() >= 3 && rings[j].size() >= 3 &&
          detail::inside(points_of(rings[j]), pts[rings[i].front()]))
        ++depth[i];

  std::vector<std::uint32_t> triangles;
  for (std::size_t outer = 0; outer < n; ++outer)
  {
    if (rings[outer].size() < 3 || depth[outer] % 2 != 0)
      continue;
    std::vector<std::uint32_t> poly = rings[outer];
    if (detail::signed_area(points_of(poly)) < 0)
      std::ranges::reverse(poly);

    // This contour's holes, each made clockwise, bridged in from the leftmost
    const auto outline = points_of(rings[outer]);
    std::vector<std::vector<std::uint32_t>> holes;
    for (std::size_t h = 0; h < n; ++h)
      if (rings[h].size() >= 3 && depth[h] == depth[outer] + 1 && detail::inside(outline, pts[rings[h].front()]))
      {
        auto hole = rings[h];
        if (detail::signed_area(points_of(hole)) > 0)
          std::ranges::reverse(hole);
        holes.push_back(std::move(hole));
      }
    auto leftmost = [&](const std::vector<std::uint32_t>& ring) {
      return std::ranges::min_element(ring, {}, [&](std::uint32_t i) { return pts[i].x(); }) - ring.begin();
    };
    std::ranges::sort(holes, {}, [&](const auto& hole) { return pts[hole[leftmost(hole)]].x(); });

    for (const auto& hole : holes)
    {
      const auto m = hole[leftmost(hole)];
      const vec2 hp = pts[m];
      // As earcut does: cast a ray from the hole's leftmost point toward -x. It can only cross downward
      // edges of the counter-clockwise outline; the crossed edge's end with the smaller x is the bridge.
      double best_x = -std::numeric_limits<double>::infinity();
      std::optional<std::size_t> bridge;
      for (std::size_t i = 0; i < poly.size(); ++i)
      {
        const vec2& a = pts[poly[i]];
        const vec2& b = pts[poly[(i + 1) % poly.size()]];
        if (!(hp.y() <= a.y() && hp.y() >= b.y() && a.y() != b.y()))
          continue;
        const double x = a.x() + (hp.y() - a.y()) * (b.x() - a.x()) / (b.y() - a.y());
        if (x <= hp.x() && x > best_x)
        {
          best_x = x;
          bridge = a.x() < b.x() ? i : (i + 1) % poly.size();
        }
      }
      if (!bridge)
        continue;
      // A vertex inside the triangle (hole point, crossing, bridge end) would block the bridge; take the one
      // closest in angle to the ray instead
      const vec2 end = pts[poly[*bridge]];
      const vec2 left{hp.y() < end.y() ? hp.x() : best_x, hp.y()};
      const vec2 right{hp.y() < end.y() ? best_x : hp.x(), hp.y()};
      double best_tan = std::numeric_limits<double>::infinity();
      for (std::size_t i = 0; i < poly.size(); ++i)
      {
        const vec2& v = pts[poly[i]];
        if (!(hp.x() >= v.x() && v.x() >= end.x() && hp.x() != v.x()))
          continue;
        if (!(detail::in_triangle(left, end, right, v) || detail::in_triangle(left, right, end, v)))
          continue;
        const double t = std::abs(hp.y() - v.y()) / (hp.x() - v.x());
        if (t < best_tan)
        {
          best_tan = t;
          bridge = i;
        }
      }
      const std::size_t at = *bridge;
      std::vector<std::uint32_t> joined(poly.begin(), poly.begin() + static_cast<std::ptrdiff_t>(at) + 1);
      const auto start = static_cast<std::size_t>(std::ranges::find(hole, m) - hole.begin());
      for (std::size_t k = 0; k <= hole.size(); ++k)
        joined.push_back(hole[(start + k) % hole.size()]);
      joined.push_back(poly[at]);
      joined.insert(joined.end(), poly.begin() + static_cast<std::ptrdiff_t>(at) + 1, poly.end());
      poly = std::move(joined);
    }

    // Ear clipping
    while (poly.size() > 3)
    {
      bool clipped = false;
      for (std::size_t i = 0; i < poly.size() && !clipped; ++i)
      {
        const std::size_t ip = (i + poly.size() - 1) % poly.size();
        const std::size_t in = (i + 1) % poly.size();
        const vec2& a = pts[poly[ip]];
        const vec2& b = pts[poly[i]];
        const vec2& c = pts[poly[in]];
        const double turn = cross2(a, b, c);
        if (turn < 0)
          continue; // reflex
        // Only a reflex vertex inside the triangle blocks the ear, as in earcut
        bool ear = true;
        if (turn > 0)
          for (std::size_t k = 0; k < poly.size() && ear; ++k)
          {
            const vec2& p = pts[poly[k]];
            if (k == ip || k == i || k == in || p == a || p == b || p == c)
              continue;
            const vec2& kp = pts[poly[(k + poly.size() - 1) % poly.size()]];
            const vec2& kn = pts[poly[(k + 1) % poly.size()]];
            ear = cross2(kp, p, kn) > 0 || !detail::in_triangle(a, b, c, p);
          }
        if (!ear)
          continue;
        if (turn > 0)
          triangles.insert(triangles.end(), {poly[ip], poly[i], poly[in]});
        poly.erase(poly.begin() + static_cast<std::ptrdiff_t>(i)); // a collinear vertex is dropped unfilled
        clipped = true;
      }
      if (!clipped)
      {
        // Self-touching input left no clean ear; cut one anyway rather than loop forever
        triangles.insert(triangles.end(), {poly[poly.size() - 1], poly[0], poly[1]});
        poly.erase(poly.begin());
      }
    }
    if (poly.size() == 3 && cross2(pts[poly[0]], pts[poly[1]], pts[poly[2]]) > 0)
      triangles.insert(triangles.end(), {poly[0], poly[1], poly[2]});
  }
  return triangles;
}

inline mesh_data merge_meshes(const std::vector<mesh_data>& meshes)
{
  mesh_data result;

  for (const auto& m : meshes)
  {
    std::uint32_t base_vertex = static_cast<std::uint32_t>(result.vertices.size());

    // Copy vertices
    result.vertices.insert(result.vertices.end(), m.vertices.begin(), m.vertices.end());

    // Copy and offset indices
    for (std::uint32_t idx : m.indices)
    {
      result.indices.push_back(base_vertex + idx);
    }
  }

  return result;
}

// ============================================================================
// Transform Mesh - Apply a transformation matrix to all vertices
// ============================================================================

inline void transform_mesh(mesh_data& mesh, const vec3& pos, const vec3& axis, const vec3& up, const vec3& scale)
{
  const auto [x_axis, y_axis, z_axis] = orientation_of(axis, up);

  for (auto& v : mesh.vertices)
  {
    // Original position
    double px = v.position[0] * scale.x();
    double py = v.position[1] * scale.y();
    double pz = v.position[2] * scale.z();

    // Rotate
    double rx = px * x_axis.x() + py * y_axis.x() + pz * z_axis.x();
    double ry = px * x_axis.y() + py * y_axis.y() + pz * z_axis.y();
    double rz = px * x_axis.z() + py * y_axis.z() + pz * z_axis.z();

    // Translate
    v.position[0] = static_cast<float>(rx + pos.x());
    v.position[1] = static_cast<float>(ry + pos.y());
    v.position[2] = static_cast<float>(rz + pos.z());

    // Rotate normal (no scale)
    double nx = v.normal[0];
    double ny = v.normal[1];
    double nz = v.normal[2];
    v.normal[0] = static_cast<float>(nx * x_axis.x() + ny * y_axis.x() + nz * z_axis.x());
    v.normal[1] = static_cast<float>(nx * x_axis.y() + ny * y_axis.y() + nz * z_axis.y());
    v.normal[2] = static_cast<float>(nx * x_axis.z() + ny * y_axis.z() + nz * z_axis.z());
  }
}

// ============================================================================
// Extrusion Mesh Generator
//
// Extrudes a 2D shape along a 3D path with optional twist and scaling.
// Uses Frenet frames (similar to generate_tube but with custom cross-section).
// ============================================================================

inline mesh_data generate_extrusion(const std::vector<vec3>& path, const std::vector<vec2>& shape, double twist = 0.0,
                                    double end_scale = 1.0, bool cap_start = true, bool cap_end = true)
{
  mesh_data mesh;

  if (path.size() < 2 || shape.empty())
    return mesh;

  int n = static_cast<int>(path.size());
  int shape_n = static_cast<int>(shape.size());

  // Compute tangents
  std::vector<vec3> tangents(n);
  for (int i = 0; i < n; ++i)
  {
    if (i == 0)
      tangents[i] = hat(path[1] - path[0]);
    else if (i == n - 1)
      tangents[i] = hat(path[n - 1] - path[n - 2]);
    else
      tangents[i] = hat(path[i + 1] - path[i - 1]);

    if (mag2(tangents[i]) < 1e-12)
      tangents[i] = vec3{1, 0, 0};
  }

  // Compute normals and binormals using parallel transport
  std::vector<vec3> normals(n);
  std::vector<vec3> binormals(n);

  vec3 up_guide{0, 1, 0};
  if (std::abs(dot(tangents[0], up_guide)) > 0.99)
    up_guide = vec3{0, 0, 1};

  normals[0] = hat(cross(tangents[0], up_guide));
  binormals[0] = cross(tangents[0], normals[0]);

  for (int i = 1; i < n; ++i)
  {
    vec3 proj = normals[i - 1] - tangents[i] * dot(normals[i - 1], tangents[i]);
    if (mag2(proj) < 1e-12)
    {
      proj = cross(tangents[i], up_guide);
      if (mag2(proj) < 1e-12)
        proj = cross(tangents[i], vec3{0, 0, 1});
    }
    normals[i] = hat(proj);
    binormals[i] = cross(tangents[i], normals[i]);
  }

  // Generate vertices at each path point
  for (int i = 0; i < n; ++i)
  {
    float t = static_cast<float>(i) / static_cast<float>(n - 1);
    double current_twist = twist * t;
    double current_scale = 1.0 + (end_scale - 1.0) * t;

    double cos_twist = std::cos(current_twist);
    double sin_twist = std::sin(current_twist);

    // Rotated frame for twist
    vec3 rot_normal = normals[i] * cos_twist + binormals[i] * sin_twist;
    vec3 rot_binormal = -normals[i] * sin_twist + binormals[i] * cos_twist;

    for (int j = 0; j < shape_n; ++j)
    {
      // 2D shape point
      double sx = shape[j].x() * current_scale;
      double sy = shape[j].y() * current_scale;

      // Transform to 3D
      vec3 pos_3d = path[i] + rot_normal * sx + rot_binormal * sy;

      // Compute normal (pointing outward from shape center)
      vec3 shape_normal = hat(rot_normal * shape[j].x() + rot_binormal * shape[j].y());

      vertex v{};
      v.position[0] = static_cast<float>(pos_3d.x());
      v.position[1] = static_cast<float>(pos_3d.y());
      v.position[2] = static_cast<float>(pos_3d.z());
      v.normal[0] = static_cast<float>(shape_normal.x());
      v.normal[1] = static_cast<float>(shape_normal.y());
      v.normal[2] = static_cast<float>(shape_normal.z());
      v.uv[0] = t;
      v.uv[1] = static_cast<float>(j) / static_cast<float>(shape_n);
      mesh.vertices.push_back(v);
    }
  }

  // Generate indices for the tube surface
  for (int i = 0; i < n - 1; ++i)
  {
    for (int j = 0; j < shape_n; ++j)
    {
      int current_ring = i * shape_n;
      int next_ring = (i + 1) * shape_n;
      int next_j = (j + 1) % shape_n;

      std::uint32_t v0 = static_cast<std::uint32_t>(current_ring + j);
      std::uint32_t v1 = static_cast<std::uint32_t>(next_ring + j);
      std::uint32_t v2 = static_cast<std::uint32_t>(current_ring + next_j);
      std::uint32_t v3 = static_cast<std::uint32_t>(next_ring + next_j);

      mesh.indices.push_back(v0);
      mesh.indices.push_back(v1);
      mesh.indices.push_back(v2);

      mesh.indices.push_back(v1);
      mesh.indices.push_back(v3);
      mesh.indices.push_back(v2);
    }
  }

  // Cap start
  if (cap_start && shape_n >= 3)
  {
    std::uint32_t center_idx = static_cast<std::uint32_t>(mesh.vertices.size());
    vec3 center_pos = path[0];
    vec3 cap_normal = tangents[0] * -1.0;

    vertex center_v{};
    center_v.position[0] = static_cast<float>(center_pos.x());
    center_v.position[1] = static_cast<float>(center_pos.y());
    center_v.position[2] = static_cast<float>(center_pos.z());
    center_v.normal[0] = static_cast<float>(cap_normal.x());
    center_v.normal[1] = static_cast<float>(cap_normal.y());
    center_v.normal[2] = static_cast<float>(cap_normal.z());
    center_v.uv[0] = 0.5f;
    center_v.uv[1] = 0.5f;
    mesh.vertices.push_back(center_v);

    // Add cap vertices with correct normal
    std::uint32_t cap_start_idx = static_cast<std::uint32_t>(mesh.vertices.size());
    for (int j = 0; j < shape_n; ++j)
    {
      vertex cap_v = mesh.vertices[j]; // Copy position from ring 0
      cap_v.normal[0] = static_cast<float>(cap_normal.x());
      cap_v.normal[1] = static_cast<float>(cap_normal.y());
      cap_v.normal[2] = static_cast<float>(cap_normal.z());
      mesh.vertices.push_back(cap_v);
    }

    // Triangle fan (reversed winding for start cap)
    for (int j = 0; j < shape_n; ++j)
    {
      int next_j = (j + 1) % shape_n;
      mesh.indices.push_back(center_idx);
      mesh.indices.push_back(cap_start_idx + static_cast<std::uint32_t>(next_j));
      mesh.indices.push_back(cap_start_idx + static_cast<std::uint32_t>(j));
    }
  }

  // Cap end
  if (cap_end && shape_n >= 3)
  {
    std::uint32_t center_idx = static_cast<std::uint32_t>(mesh.vertices.size());
    vec3 center_pos = path[n - 1];
    vec3 cap_normal = tangents[n - 1];

    vertex center_v{};
    center_v.position[0] = static_cast<float>(center_pos.x());
    center_v.position[1] = static_cast<float>(center_pos.y());
    center_v.position[2] = static_cast<float>(center_pos.z());
    center_v.normal[0] = static_cast<float>(cap_normal.x());
    center_v.normal[1] = static_cast<float>(cap_normal.y());
    center_v.normal[2] = static_cast<float>(cap_normal.z());
    center_v.uv[0] = 0.5f;
    center_v.uv[1] = 0.5f;
    mesh.vertices.push_back(center_v);

    // Add cap vertices with correct normal
    std::uint32_t cap_start_idx = static_cast<std::uint32_t>(mesh.vertices.size());
    int last_ring = (n - 1) * shape_n;
    for (int j = 0; j < shape_n; ++j)
    {
      vertex cap_v = mesh.vertices[last_ring + j]; // Copy position from last ring
      cap_v.normal[0] = static_cast<float>(cap_normal.x());
      cap_v.normal[1] = static_cast<float>(cap_normal.y());
      cap_v.normal[2] = static_cast<float>(cap_normal.z());
      mesh.vertices.push_back(cap_v);
    }

    // Triangle fan
    for (int j = 0; j < shape_n; ++j)
    {
      int next_j = (j + 1) % shape_n;
      mesh.indices.push_back(center_idx);
      mesh.indices.push_back(cap_start_idx + static_cast<std::uint32_t>(j));
      mesh.indices.push_back(cap_start_idx + static_cast<std::uint32_t>(next_j));
    }
  }

  return mesh;
}

// ============================================================================
// Object Mesh Generation
//
// Generate meshes for vcpp objects with their transforms applied.
// Used for compound object creation.
// ============================================================================

namespace object_mesh
{

// Generate mesh for a sphere
inline mesh_data generate_sphere_mesh(const vec3& pos, double radius, const vec3& axis, const vec3& up)
{
  auto mesh = generate_sphere();
  transform_mesh(mesh, pos, axis, up, vec3{radius * 2.0, radius * 2.0, radius * 2.0});
  return mesh;
}

// Generate mesh for a box
inline mesh_data generate_box_mesh(const vec3& pos, double length, double height, double width, const vec3& axis,
                                   const vec3& up)
{
  auto mesh = generate_box();
  transform_mesh(mesh, pos, axis, up, vec3{length, height, width});
  return mesh;
}

// Generate mesh for a cylinder
inline mesh_data generate_cylinder_mesh(const vec3& pos, double radius, double length, const vec3& axis,
                                        const vec3& up)
{
  auto mesh = generate_cylinder();
  // Cylinder mesh is aligned along X, length 1, diameter 1
  transform_mesh(mesh, pos, axis, up, vec3{length, radius * 2.0, radius * 2.0});
  return mesh;
}

// Generate mesh for a cone
inline mesh_data generate_cone_mesh(const vec3& pos, double radius, double length, const vec3& axis, const vec3& up)
{
  auto mesh = generate_cone();
  // Cone mesh is aligned along X, length 1, diameter 1
  transform_mesh(mesh, pos, axis, up, vec3{length, radius * 2.0, radius * 2.0});
  return mesh;
}

// Generate mesh for a pyramid
inline mesh_data generate_pyramid_mesh(const vec3& pos, double length, double height, double width, const vec3& axis,
                                       const vec3& up)
{
  auto mesh = generate_pyramid();
  transform_mesh(mesh, pos, axis, up, vec3{length, height, width});
  return mesh;
}

} // namespace object_mesh

} // namespace vcpp::mesh
