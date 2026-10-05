/*
 *  test_meshes.cpp - Generated meshes have the shape their parameters describe, and GlowScript's
 *  texture coordinates
 */

import vcpp;
import std;

using namespace vcpp;

namespace
{

// A helix of length 4, radius 1, wire radius 0.1: x spans [0, 4] give or take the wire, y and z reach
// radius + wire; it starts at +z, and coils sets the number of steps (60 per coil)
bool test_helix_shape()
{
  const auto m = mesh::generate_helix(4.0f, 1.0f, 0.1f, 3, true, 8, 60);
  float xmin = 1e9f;
  float xmax = -1e9f;
  float reach = 0;
  for (const auto& v : m.vertices)
  {
    xmin = std::min(xmin, v.position[0]);
    xmax = std::max(xmax, v.position[0]);
    reach = std::max({reach, std::abs(v.position[1]), std::abs(v.position[2])});
  }
  // The first ring of vertices surrounds the path's start, which should be (0, 0, radius)
  float zsum = 0;
  for (int j = 0; j < 8; ++j)
    zsum += m.vertices[static_cast<std::size_t>(j)].position[2];
  const bool starts_at_z = std::abs(zsum / 8 - 1.0f) < 0.02f;
  return xmin > -0.11f && xmax < 4.11f && std::abs(reach - 1.1f) < 1e-3f && starts_at_z &&
         m.vertices.size() == static_cast<std::size_t>((3 * 60 + 1) * 9);
}

// A point on a wavy path, so consecutive tangents differ
vec3 path_point(int i) { return vec3{0.1 * i, std::sin(0.3 * i), std::cos(0.2 * i)}; }

// The centres of the rings the draw ranges actually use
std::vector<vec3> drawn_centers(const mesh::tube_builder& tube)
{
  std::set<std::uint32_t> rings;
  for (const auto& [first, count] : tube.draw_ranges())
    for (std::uint32_t k = first; k < first + count; ++k)
      rings.insert(static_cast<std::uint32_t>(tube.indices()[k] / tube.ring_size()));
  std::vector<vec3> centers;
  for (auto r : rings)
  {
    vec3 sum{0, 0, 0};
    const std::size_t slices = tube.ring_size() - 1; // the last vertex repeats the first
    for (std::size_t j = 0; j < slices; ++j)
    {
      const auto& v = tube.vertices()[r * tube.ring_size() + j];
      sum = sum + vec3{v.position[0], v.position[1], v.position[2]};
    }
    centers.push_back(sum / static_cast<double>(slices));
  }
  return centers;
}

// With retain, after warm-up each new point rewrites two rings, nothing is rebuilt, and the drawn tube
// runs through exactly the newest `retain` points
bool test_tube_ring_buffer()
{
  mesh::tube_builder tube(6);
  std::vector<vec3> points;
  int rebuilds = 0;
  bool two_rings_each_time = true;
  for (int i = 0; i < 1000; ++i)
  {
    points.push_back(path_point(i));
    if (points.size() > 50)
      points.erase(points.begin());
    const bool rebuilt = tube.update(points, static_cast<std::uint64_t>(i + 1), 50, 0.05f);
    rebuilds += rebuilt;
    if (i > 2 && !rebuilt && tube.changed_rings().size() != 2)
      two_rings_each_time = false;
  }
  const auto centers = drawn_centers(tube);
  bool all_match = centers.size() == points.size();
  for (const auto& c : centers)
    all_match = all_match && std::ranges::any_of(points, [&](const vec3& p) { return mag(c - p) < 1e-5; });
  const auto ranges = tube.draw_ranges();
  return rebuilds == 1 && two_rings_each_time && all_match && ranges[0].second + ranges[1].second == 49 * 36;
}

// Without retain the buffer doubles, so 1000 points rebuild only a handful of times
bool test_tube_growth()
{
  mesh::tube_builder tube(6);
  std::vector<vec3> points;
  int rebuilds = 0;
  for (int i = 0; i < 1000; ++i)
  {
    points.push_back(path_point(i));
    rebuilds += tube.update(points, static_cast<std::uint64_t>(i + 1), -1, 0.05f);
  }
  return rebuilds <= 6 && drawn_centers(tube).size() == 1000;
}

// A trail that restarts (its count goes back down) is rebuilt
bool test_tube_restart()
{
  mesh::tube_builder tube(6);
  std::vector<vec3> points{path_point(0), path_point(1), path_point(2)};
  tube.update(points, 3, -1, 0.05f);
  points = {path_point(5), path_point(6)};
  return tube.update(points, 2, -1, 0.05f) && drawn_centers(tube).size() == 2;
}

// Texture coordinates are GlowScript's, where v = 1 is the top of the image. The sphere's first vertex is
// the north pole at the image's top left; its equator starts at -z, the image's left edge; the box's front
// face has the image's top left at its top left; the cylinder's side runs the image along its axis.
bool test_texture_coordinates()
{
  auto near = [](float a, float b) { return std::abs(a - b) < 1e-4f; };
  const auto sphere = mesh::generate_sphere(30);
  const auto& pole = sphere.vertices[0];
  const auto& equator = sphere.vertices[15 * 31];
  const bool sphere_ok = near(pole.position[1], 0.5f) && near(pole.uv[0], 0) && near(pole.uv[1], 1) &&
                         near(equator.position[2], -0.5f) && near(equator.uv[0], 0) && near(equator.uv[1], 0.5f);

  const auto box = mesh::generate_box();
  bool box_ok = false;
  for (const auto& v : box.vertices)
    if (near(v.normal[2], 1) && near(v.position[0], -0.5f) && near(v.position[1], 0.5f))
      box_ok = near(v.uv[0], 0) && near(v.uv[1], 1);

  const auto cylinder = mesh::generate_cylinder(50);
  bool cylinder_ok = true;
  for (const auto& v : cylinder.vertices)
    if (v.normal[0] == 0) // the side
      cylinder_ok = cylinder_ok && near(v.uv[0], v.position[0]);
  return sphere_ok && box_ok && cylinder_ok;
}

} // namespace

int main()
{
  int passed = 0;
  int failed = 0;

  auto run_test = [&](const char* name, bool (*test)()) {
    if (test())
    {
      std::println("  ✓ {}", name);
      ++passed;
    }
    else
    {
      std::println("  ✗ {} FAILED", name);
      ++failed;
    }
  };

  std::println("vcpp mesh tests");
  std::println("===============");

  run_test("helix shape", test_helix_shape);
  run_test("tube ring buffer", test_tube_ring_buffer);
  run_test("tube growth", test_tube_growth);
  run_test("tube restart", test_tube_restart);
  run_test("texture coordinates", test_texture_coordinates);

  std::println("===============");
  std::println("Passed: {}/{}", passed, passed + failed);

  return failed > 0 ? 1 : 0;
}
