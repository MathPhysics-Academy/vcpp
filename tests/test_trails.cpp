/*
 *  test_trails.cpp - Trails follow GlowScript's attach_trail
 */

import vcpp;
import std;

using namespace vcpp;

namespace
{

// retain counts points: only the newest are kept
bool test_retain_counts_points()
{
  canvas c;
  auto ball = c.add(sphere(make_trail = true, retain = 3));
  for (int i = 1; i <= 5; ++i)
  {
    ball->m_pos = vec3{static_cast<double>(i), 0, 0};
    c.update_trails();
  }
  const auto& points = c.m_trails[ball.entry()].positions;
  return points.size() == 3 && points.front() == vec3{3, 0, 0} && points.back() == vec3{5, 0, 0};
}

// A point is added only when the object has moved, and not while it is invisible
bool test_only_moves_add_points()
{
  canvas c;
  auto ball = c.add(sphere(make_trail = true));
  c.update_trails();
  c.update_trails();
  ball->m_visible = false;
  ball->m_pos = vec3{1, 0, 0};
  c.update_trails();
  return c.m_trails[ball.entry()].positions.size() == 1;
}

// The trail takes the object's colour unless trail_color is given
bool test_trail_color_default()
{
  auto plain = sphere(color = colors::green, make_trail = true);
  auto chosen = sphere(color = colors::green, make_trail = true, trail_color = colors::red);
  return plain.m_trail_color == colors::green && chosen.m_trail_color == colors::red;
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

  std::println("vcpp trail tests");
  std::println("================");

  run_test("retain counts points", test_retain_counts_points);
  run_test("only moves add points", test_only_moves_add_points);
  run_test("trail colour defaults to the object's", test_trail_color_default);

  std::println("================");
  std::println("Passed: {}/{}", passed, passed + failed);

  return failed > 0 ? 1 : 0;
}
