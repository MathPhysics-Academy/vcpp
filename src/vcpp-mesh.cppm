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
// Sphere Mesh Generator
//
// Unit sphere (radius 0.5, diameter 1.0) for scaling via instance matrix.
// Poles at Y axis.
// ============================================================================

inline mesh_data generate_sphere(int slices = 16, int stacks = 12)
{
  mesh_data mesh;

  for (int i = 0; i <= stacks; ++i)
  {
    float phi = PI * static_cast<float>(i) / static_cast<float>(stacks);
    float y = std::cos(phi);
    float r = std::sin(phi);

    for (int j = 0; j <= slices; ++j)
    {
      float theta = TWO_PI * static_cast<float>(j) / static_cast<float>(slices);
      float x = r * std::cos(theta);
      float z = r * std::sin(theta);

      vertex v{};
      v.position[0] = x * 0.5f;
      v.position[1] = y * 0.5f;
      v.position[2] = z * 0.5f;
      v.normal[0] = x;
      v.normal[1] = y;
      v.normal[2] = z;
      v.uv[0] = static_cast<float>(j) / static_cast<float>(slices);
      v.uv[1] = static_cast<float>(i) / static_cast<float>(stacks);
      mesh.vertices.push_back(v);
    }
  }

  for (int i = 0; i < stacks; ++i)
  {
    for (int j = 0; j < slices; ++j)
    {
      std::uint32_t a = static_cast<std::uint32_t>(i * (slices + 1) + j);
      std::uint32_t b = a + static_cast<std::uint32_t>(slices + 1);

      mesh.indices.push_back(a);
      mesh.indices.push_back(b);
      mesh.indices.push_back(a + 1);

      mesh.indices.push_back(a + 1);
      mesh.indices.push_back(b);
      mesh.indices.push_back(b + 1);
    }
  }

  return mesh;
}

// ============================================================================
// Box Mesh Generator
//
// Unit box (1x1x1) centered at origin.
// ============================================================================

inline mesh_data generate_box()
{
  mesh_data mesh;
  float n = 0.5f;

  auto add_face = [&](float nx, float ny, float nz, float ax, float ay, float az, float bx, float by, float bz) {
    float cx = nx * n, cy = ny * n, cz = nz * n;
    float corners[4][3] = {{cx - ax - bx, cy - ay - by, cz - az - bz},
                           {cx + ax - bx, cy + ay - by, cz + az - bz},
                           {cx + ax + bx, cy + ay + by, cz + az + bz},
                           {cx - ax + bx, cy - ay + by, cz - az + bz}};
    float uvs[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};

    for (int i = 0; i < 4; ++i)
    {
      vertex v{};
      v.position[0] = corners[i][0];
      v.position[1] = corners[i][1];
      v.position[2] = corners[i][2];
      v.normal[0] = nx;
      v.normal[1] = ny;
      v.normal[2] = nz;
      v.uv[0] = uvs[i][0];
      v.uv[1] = uvs[i][1];
      mesh.vertices.push_back(v);
    }
  };

  add_face(1, 0, 0, 0, n, 0, 0, 0, n);  // +X
  add_face(-1, 0, 0, 0, n, 0, 0, 0, -n); // -X
  add_face(0, 1, 0, n, 0, 0, 0, 0, n);  // +Y
  add_face(0, -1, 0, n, 0, 0, 0, 0, -n); // -Y
  add_face(0, 0, 1, n, 0, 0, 0, n, 0);  // +Z
  add_face(0, 0, -1, -n, 0, 0, 0, n, 0); // -Z

  for (std::uint32_t f = 0; f < 6; ++f)
  {
    std::uint32_t base = f * 4;
    mesh.indices.push_back(base);
    mesh.indices.push_back(base + 1);
    mesh.indices.push_back(base + 2);
    mesh.indices.push_back(base);
    mesh.indices.push_back(base + 2);
    mesh.indices.push_back(base + 3);
  }

  return mesh;
}

// ============================================================================
// Cylinder Mesh Generator
//
// Aligned along +X axis (0 to 1), radius 0.5 (diameter 1.0).
// pos is base center, axis points to top.
// ============================================================================

inline mesh_data generate_cylinder(int slices = 16)
{
  mesh_data mesh;

  // Body
  for (int i = 0; i <= slices; ++i)
  {
    float theta = TWO_PI * static_cast<float>(i) / static_cast<float>(slices);
    float y = std::cos(theta);
    float z = std::sin(theta);
    float u = static_cast<float>(i) / static_cast<float>(slices);

    // Base vertex (x=0)
    vertex v0{};
    v0.position[0] = 0.0f;
    v0.position[1] = y * 0.5f;
    v0.position[2] = z * 0.5f;
    v0.normal[0] = 0.0f;
    v0.normal[1] = y;
    v0.normal[2] = z;
    v0.uv[0] = u;
    v0.uv[1] = 0.0f;
    mesh.vertices.push_back(v0);

    // Top vertex (x=1)
    vertex v1{};
    v1.position[0] = 1.0f;
    v1.position[1] = y * 0.5f;
    v1.position[2] = z * 0.5f;
    v1.normal[0] = 0.0f;
    v1.normal[1] = y;
    v1.normal[2] = z;
    v1.uv[0] = u;
    v1.uv[1] = 1.0f;
    mesh.vertices.push_back(v1);
  }

  // Base Cap center
  std::uint32_t base_center_idx = static_cast<std::uint32_t>(mesh.vertices.size());
  mesh.vertices.push_back(vertex{{0, 0, 0}, {-1, 0, 0}, {0.5f, 0.5f}});
  for (int i = 0; i <= slices; ++i)
  {
    float theta = TWO_PI * static_cast<float>(i) / static_cast<float>(slices);
    float y = std::cos(theta);
    float z = std::sin(theta);
    mesh.vertices.push_back(
      vertex{{0, y * 0.5f, z * 0.5f}, {-1, 0, 0}, {0.5f + y * 0.5f, 0.5f + z * 0.5f}});
  }

  // Top Cap center
  std::uint32_t top_center_idx = static_cast<std::uint32_t>(mesh.vertices.size());
  mesh.vertices.push_back(vertex{{1, 0, 0}, {1, 0, 0}, {0.5f, 0.5f}});
  for (int i = 0; i <= slices; ++i)
  {
    float theta = TWO_PI * static_cast<float>(i) / static_cast<float>(slices);
    float y = std::cos(theta);
    float z = std::sin(theta);
    mesh.vertices.push_back(
      vertex{{1, y * 0.5f, z * 0.5f}, {1, 0, 0}, {0.5f + y * 0.5f, 0.5f + z * 0.5f}});
  }

  // Body indices
  for (int i = 0; i < slices; ++i)
  {
    std::uint32_t base = static_cast<std::uint32_t>(i * 2);
    mesh.indices.push_back(base);
    mesh.indices.push_back(base + 1);
    mesh.indices.push_back(base + 2);
    mesh.indices.push_back(base + 1);
    mesh.indices.push_back(base + 3);
    mesh.indices.push_back(base + 2);
  }

  // Base Cap indices (reversed winding)
  for (int i = 0; i < slices; ++i)
  {
    mesh.indices.push_back(base_center_idx);
    mesh.indices.push_back(base_center_idx + 1 + static_cast<std::uint32_t>(i) + 1);
    mesh.indices.push_back(base_center_idx + 1 + static_cast<std::uint32_t>(i));
  }

  // Top Cap indices
  for (int i = 0; i < slices; ++i)
  {
    mesh.indices.push_back(top_center_idx);
    mesh.indices.push_back(top_center_idx + 1 + static_cast<std::uint32_t>(i));
    mesh.indices.push_back(top_center_idx + 1 + static_cast<std::uint32_t>(i) + 1);
  }

  return mesh;
}

// ============================================================================
// Cone Mesh Generator
//
// Aligned along +X axis (0 to 1), base radius 0.5 (diameter 1.0).
// Base at x=0, tip at x=1.
// ============================================================================

inline mesh_data generate_cone(int slices = 16)
{
  mesh_data mesh;

  // Body vertices (base rim)
  for (int i = 0; i <= slices; ++i)
  {
    float theta = TWO_PI * static_cast<float>(i) / static_cast<float>(slices);
    float y = std::cos(theta);
    float z = std::sin(theta);

    // Normal calculation: perpendicular to cone surface
    vec3 n{0.5, y, z};
    n = hat(n);

    vertex v{};
    v.position[0] = 0.0f;
    v.position[1] = y * 0.5f;
    v.position[2] = z * 0.5f;
    v.normal[0] = static_cast<float>(n.x());
    v.normal[1] = static_cast<float>(n.y());
    v.normal[2] = static_cast<float>(n.z());
    v.uv[0] = static_cast<float>(i) / static_cast<float>(slices);
    v.uv[1] = 0.0f;
    mesh.vertices.push_back(v);
  }

  // Tip vertex
  std::uint32_t tip_idx = static_cast<std::uint32_t>(mesh.vertices.size());
  mesh.vertices.push_back(vertex{{1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.5f, 1.0f}});

  // Base Cap center
  std::uint32_t base_center_idx = static_cast<std::uint32_t>(mesh.vertices.size());
  mesh.vertices.push_back(vertex{{0, 0, 0}, {-1, 0, 0}, {0.5f, 0.5f}});
  for (int i = 0; i <= slices; ++i)
  {
    float theta = TWO_PI * static_cast<float>(i) / static_cast<float>(slices);
    float y = std::cos(theta);
    float z = std::sin(theta);
    mesh.vertices.push_back(
      vertex{{0, y * 0.5f, z * 0.5f}, {-1, 0, 0}, {0.5f + y * 0.5f, 0.5f + z * 0.5f}});
  }

  // Body indices (triangles to tip)
  for (int i = 0; i < slices; ++i)
  {
    mesh.indices.push_back(static_cast<std::uint32_t>(i));
    mesh.indices.push_back(tip_idx);
    mesh.indices.push_back(static_cast<std::uint32_t>(i + 1));
  }

  // Base Cap indices
  for (int i = 0; i < slices; ++i)
  {
    mesh.indices.push_back(base_center_idx);
    mesh.indices.push_back(base_center_idx + 1 + static_cast<std::uint32_t>(i) + 1);
    mesh.indices.push_back(base_center_idx + 1 + static_cast<std::uint32_t>(i));
  }

  return mesh;
}

// ============================================================================
// Helix Mesh Generator
//
// Coiled tube along X axis.
// Length 1.0, helix radius 1.0, tube radius 0.1.
// ============================================================================

inline mesh_data generate_helix(int coils = 8, int tube_slices = 8, int steps_per_coil = 16)
{
  mesh_data mesh;
  float length = 1.0f;
  float helix_radius = 1.0f;
  float tube_radius = 0.1f;

  int total_steps = coils * steps_per_coil;

  for (int i = 0; i <= total_steps; ++i)
  {
    float t = static_cast<float>(i) / static_cast<float>(total_steps);
    float angle = t * static_cast<float>(coils) * TWO_PI;
    float x_center = t * length;

    // Helix path point
    float y_center = helix_radius * std::cos(angle);
    float z_center = helix_radius * std::sin(angle);

    // Tangent vector
    float A = static_cast<float>(coils) * TWO_PI;
    float tx = length;
    float ty = -helix_radius * A * std::sin(angle);
    float tz = helix_radius * A * std::cos(angle);
    float t_len = std::sqrt(tx * tx + ty * ty + tz * tz);
    tx /= t_len;
    ty /= t_len;
    tz /= t_len;

    // Normal (radial direction)
    float nx = 0;
    float ny = std::cos(angle);
    float nz = std::sin(angle);

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

inline mesh_data generate_ring(int major_segments = 32, int minor_segments = 12)
{
  mesh_data mesh;
  float major_radius = 1.0f;
  float minor_radius = 0.1f;

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
// Tube Mesh Generator (for curves)
//
// Generates a tube along arbitrary 3D points using Frenet frames.
// ============================================================================

inline mesh_data generate_tube(const std::vector<vec3>& points, float radius, int tube_slices = 8)
{
  mesh_data mesh;

  if (points.size() < 2)
    return mesh;

  int n = static_cast<int>(points.size());

  // Compute tangents
  std::vector<vec3> tangents(n);
  for (int i = 0; i < n; ++i)
  {
    if (i == 0)
      tangents[i] = hat(points[1] - points[0]);
    else if (i == n - 1)
      tangents[i] = hat(points[n - 1] - points[n - 2]);
    else
      tangents[i] = hat(points[i + 1] - points[i - 1]);

    if (mag2(tangents[i]) < 1e-12)
      tangents[i] = vec3{1, 0, 0};
  }

  // Compute initial normal
  std::vector<vec3> normals(n);
  std::vector<vec3> binormals(n);

  vec3 up_guide{0, 1, 0};
  if (std::abs(dot(tangents[0], up_guide)) > 0.99)
    up_guide = vec3{0, 0, 1};

  normals[0] = hat(cross(tangents[0], up_guide));
  binormals[0] = cross(tangents[0], normals[0]);

  // Propagate frame (parallel transport)
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

  // Generate ring of vertices at each point
  for (int i = 0; i < n; ++i)
  {
    float t_param = static_cast<float>(i) / static_cast<float>(n - 1);

    for (int j = 0; j <= tube_slices; ++j)
    {
      double phi = TWO_PI * static_cast<double>(j) / static_cast<double>(tube_slices);
      double cos_phi = std::cos(phi);
      double sin_phi = std::sin(phi);

      vec3 offset = normals[i] * cos_phi + binormals[i] * sin_phi;
      vec3 pos = points[i] + offset * static_cast<double>(radius);

      vertex v{};
      v.position[0] = static_cast<float>(pos.x());
      v.position[1] = static_cast<float>(pos.y());
      v.position[2] = static_cast<float>(pos.z());
      v.normal[0] = static_cast<float>(offset.x());
      v.normal[1] = static_cast<float>(offset.y());
      v.normal[2] = static_cast<float>(offset.z());
      v.uv[0] = t_param;
      v.uv[1] = static_cast<float>(j) / static_cast<float>(tube_slices);
      mesh.vertices.push_back(v);
    }
  }

  // Generate indices
  for (int i = 0; i < n - 1; ++i)
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
// Pyramid Mesh Generator
//
// Apex at x=1, base at x=0, base is 1x1 in YZ plane.
// ============================================================================

inline mesh_data generate_pyramid()
{
  mesh_data mesh;

  // Apex
  float apex_x = 1.0f;
  // Base corners
  float base_x = 0.0f;
  float half = 0.5f;

  // Base vertices (for base face)
  vertex base_verts[4] = {
    {{base_x, -half, -half}, {-1, 0, 0}, {0, 0}},
    {{base_x, half, -half}, {-1, 0, 0}, {1, 0}},
    {{base_x, half, half}, {-1, 0, 0}, {1, 1}},
    {{base_x, -half, half}, {-1, 0, 0}, {0, 1}},
  };

  // Base face
  std::uint32_t base_start = static_cast<std::uint32_t>(mesh.vertices.size());
  for (int i = 0; i < 4; ++i)
    mesh.vertices.push_back(base_verts[i]);
  mesh.indices.push_back(base_start);
  mesh.indices.push_back(base_start + 2);
  mesh.indices.push_back(base_start + 1);
  mesh.indices.push_back(base_start);
  mesh.indices.push_back(base_start + 3);
  mesh.indices.push_back(base_start + 2);

  // Side faces
  vec3 corners[4] = {
    {base_x, -half, -half},
    {base_x, half, -half},
    {base_x, half, half},
    {base_x, -half, half},
  };
  vec3 apex_vec{apex_x, 0, 0};

  for (int i = 0; i < 4; ++i)
  {
    int next = (i + 1) % 4;
    vec3 v0 = corners[i];
    vec3 v1 = corners[next];
    vec3 edge1 = apex_vec - v0;
    vec3 edge2 = v1 - v0;
    vec3 n = hat(cross(edge2, edge1));

    std::uint32_t tri_start = static_cast<std::uint32_t>(mesh.vertices.size());

    vertex vv0{};
    vv0.position[0] = static_cast<float>(v0.x());
    vv0.position[1] = static_cast<float>(v0.y());
    vv0.position[2] = static_cast<float>(v0.z());
    vv0.normal[0] = static_cast<float>(n.x());
    vv0.normal[1] = static_cast<float>(n.y());
    vv0.normal[2] = static_cast<float>(n.z());
    vv0.uv[0] = 0;
    vv0.uv[1] = 0;
    mesh.vertices.push_back(vv0);

    vertex vv1{};
    vv1.position[0] = static_cast<float>(v1.x());
    vv1.position[1] = static_cast<float>(v1.y());
    vv1.position[2] = static_cast<float>(v1.z());
    vv1.normal[0] = static_cast<float>(n.x());
    vv1.normal[1] = static_cast<float>(n.y());
    vv1.normal[2] = static_cast<float>(n.z());
    vv1.uv[0] = 1;
    vv1.uv[1] = 0;
    mesh.vertices.push_back(vv1);

    vertex apex_v{};
    apex_v.position[0] = static_cast<float>(apex_vec.x());
    apex_v.position[1] = static_cast<float>(apex_vec.y());
    apex_v.position[2] = static_cast<float>(apex_vec.z());
    apex_v.normal[0] = static_cast<float>(n.x());
    apex_v.normal[1] = static_cast<float>(n.y());
    apex_v.normal[2] = static_cast<float>(n.z());
    apex_v.uv[0] = 0.5f;
    apex_v.uv[1] = 1;
    mesh.vertices.push_back(apex_v);

    mesh.indices.push_back(tri_start);
    mesh.indices.push_back(tri_start + 1);
    mesh.indices.push_back(tri_start + 2);
  }

  return mesh;
}

// ============================================================================
// Merge Meshes - Combine multiple meshes into one
//
// Used for compound objects. Concatenates vertices and adjusts indices.
// ============================================================================

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
  // Build rotation matrix from axis and up
  vec3 x_axis = hat(axis);
  vec3 z_axis = hat(cross(x_axis, up));
  vec3 y_axis = cross(z_axis, x_axis);

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
