/*
 *  test_meshes.cpp - Generated meshes have the shape their parameters describe
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

  std::println("===============");
  std::println("Passed: {}/{}", passed, passed + failed);

  return failed > 0 ? 1 : 0;
}
