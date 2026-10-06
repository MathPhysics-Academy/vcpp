/*
 *  test_shapes.cpp - shapes:: and paths:: give GlowScript's points
 *
 *  The expected values are GlowScript 3.2's own (shapespaths.js) for the same calls.
 */

import vcpp;
import std;

using namespace vcpp;

namespace
{

bool near(double a, double b) { return std::abs(a - b) < 1e-9; }

bool near(const vec2& a, double x, double y) { return near(a.x(), x) && near(a.y(), y); }

bool near(const vec3& a, double x, double y, double z) { return near(a.x(), x) && near(a.y(), y) && near(a.z(), z); }

// A circle of 16 points closes on its first; thickness makes a frame: the outline, then the hole
bool test_circle_and_frame()
{
  const auto c = shapes::circle(radius = 2.0, shapes::np = 16);
  const auto f = shapes::rectangle(width = 2.0, thickness = 0.1);
  return c.size() == 1 && c[0].size() == 17 && near(c[0][1], 1.8477590650225735, 0.7653668647301796) &&
         c[0].back() == c[0].front() && f.size() == 2 && f[1].size() == 5 && near(f[1][1], 0.8, 0.8);
}

// A star starts at the top; a gear's outline keeps GlowScript's number of points
bool test_star_and_gear()
{
  const auto s = shapes::star();
  const auto g = shapes::gear();
  return s[0].size() == 11 && near(s[0][1], -0.29389262614623657, 0.4045084971874737) && g[0].size() == 172 &&
         near(g[0][1], 0.8970038131759748, 0.0705990438303291);
}

// Paths lie in the xz plane, the shape's y becoming -z; up turns them
bool test_paths()
{
  const auto arc = paths::arc(radius = 1.7, shapes::angle2 = std::numbers::pi);
  const auto ring = paths::circle(radius = 1.0, up = vec3{1, 0, 0});
  return arc.size() == 34 && near(arc.front(), 1.7, 0, 0) && near(arc[8], 1.2303478647786192, 0, -1.1731343195195902) &&
         ring.size() == 65 && near(ring.front(), 6.123233995736766e-17, -1, 0);
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

  std::println("vcpp shapes and paths tests");
  std::println("===========================");

  run_test("circle and frame", test_circle_and_frame);
  run_test("star and gear", test_star_and_gear);
  run_test("paths", test_paths);

  std::println("===========================");
  std::println("Passed: {}/{}", passed, passed + failed);

  return failed > 0 ? 1 : 0;
}
