/*
 *  test_orientation.cpp - Object orientation follows GlowScript: x = axis, y = up
 */

import vcpp;
import std;

using namespace vcpp;

namespace
{

bool near(const vec3& a, const vec3& b) { return mag(vec3{a.x() - b.x(), a.y() - b.y(), a.z() - b.z()}) < 1e-12; }

bool test_default_is_identity()
{
  auto [x, y, z] = orientation_of(vec3{1, 0, 0}, vec3{0, 1, 0});
  return near(x, vec3{1, 0, 0}) && near(y, vec3{0, 1, 0}) && near(z, vec3{0, 0, 1});
}

// The axis sets x, whatever its length; z completes a right-handed frame
bool test_axis_along_z()
{
  auto [x, y, z] = orientation_of(vec3{0, 0, 5}, vec3{0, 1, 0});
  return near(x, vec3{0, 0, 1}) && near(y, vec3{0, 1, 0}) && near(z, vec3{-1, 0, 0});
}

// up parallel to axis: GlowScript would have turned up with the axis, from (0,1,0) to (1,0,0)
bool test_axis_parallel_to_up()
{
  auto [x, y, z] = orientation_of(vec3{0, -1.2, 0}, vec3{0, 1, 0});
  return near(x, vec3{0, -1, 0}) && near(y, vec3{1, 0, 0}) && near(z, vec3{0, 0, 1});
}

// An up that isn't perpendicular to axis is straightened, not used as given
bool test_up_made_perpendicular()
{
  auto [x, y, z] = orientation_of(vec3{1, 0, 0}, vec3{1, 1, 0});
  return near(y, vec3{0, 1, 0});
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

  std::println("vcpp orientation tests");
  std::println("======================");

  run_test("default is identity", test_default_is_identity);
  run_test("axis along z", test_axis_along_z);
  run_test("axis parallel to up", test_axis_parallel_to_up);
  run_test("up made perpendicular", test_up_made_perpendicular);

  std::println("======================");
  std::println("Passed: {}/{}", passed, passed + failed);

  return failed > 0 ? 1 : 0;
}
