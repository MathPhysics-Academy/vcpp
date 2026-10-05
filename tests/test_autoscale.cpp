/*
 *  test_autoscale.cpp - The camera fits the scene as GlowScript's autoscale does
 */

import vcpp;
import std;

using namespace vcpp;

namespace
{

bool near(double a, double b) { return std::abs(a - b) < 1e-3; }

double distance(const canvas& c) { return mag(c.m_camera.m_pos - c.m_camera.m_center); }

// A unit sphere at the origin on an 800x600 canvas, 60-degree fov: range = 1.1(1 + tan 30) and the
// camera sits at range / tan 30, along its existing direction
bool test_fits_a_sphere()
{
  canvas c;
  c.add(sphere(radius = 1.0));
  c.autoscale(800, 600);
  return near(distance(c), 1.1 * (1 + std::tan(std::numbers::pi / 6)) / std::tan(std::numbers::pi / 6)) &&
         near(c.m_camera.m_pos.x(), 0) && near(c.m_camera.m_pos.y(), 0) && c.m_camera.m_pos.z() > 0;
}

// It refits when the scene grows, not when it shrinks a little, and again when it shrinks below a third
bool test_refit_rule()
{
  canvas c;
  auto ball = c.add(sphere(radius = 1.0));
  c.autoscale(800, 600);
  const double small = distance(c);
  ball->m_radius = 4.0;
  c.autoscale(800, 600);
  const double grown = distance(c);
  ball->m_radius = 2.0;
  c.autoscale(800, 600);
  const double after_small_shrink = distance(c);
  ball->m_radius = 0.5;
  c.autoscale(800, 600);
  const double after_big_shrink = distance(c);
  return grown > small && near(after_small_shrink, grown) && after_big_shrink < grown;
}

// Off means the camera is left alone; clear() turns it back on
bool test_off_and_clear()
{
  canvas c;
  c.add(sphere(radius = 1.0));
  c.m_autoscale = false;
  c.autoscale(800, 600);
  const bool untouched = near(distance(c), 10);
  c.clear();
  c.add(sphere(radius = 1.0));
  c.autoscale(800, 600);
  return untouched && c.m_autoscale && distance(c) < 10;
}

// center moves the camera with it; forward turns it about center; both keep its distance
bool test_center_and_forward()
{
  canvas c;
  c.set_center(vec3{1, 2, 3});
  const bool moved = near(distance(c), 10) && near(c.m_camera.m_pos.z(), 13) && near(c.m_camera.m_pos.x(), 1);
  c.set_forward(vec3{0, -1, 1}); // looking down at 45 degrees, toward +z
  const vec3 off = c.m_camera.m_pos - c.m_camera.m_center;
  return moved && near(distance(c), 10) && near(off.y(), 10 / std::sqrt(2.0)) && near(off.z(), -off.y());
}

// range places the camera at the next render, range / tan 30 away on a wide canvas and further by
// height/width on a tall one, and ends autoscale
bool test_range()
{
  canvas c;
  c.add(sphere(radius = 1.0));
  c.set_range(2);
  c.autoscale(800, 600);
  const bool wide = near(distance(c), 2 / std::tan(std::numbers::pi / 6)) && !c.m_autoscale;
  c.set_range(2);
  c.autoscale(600, 800);
  return wide && near(distance(c), 2 * (800.0 / 600) / std::tan(std::numbers::pi / 6));
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

  std::println("vcpp autoscale tests");
  std::println("====================");

  run_test("fits a sphere", test_fits_a_sphere);
  run_test("refit rule", test_refit_rule);
  run_test("off and clear", test_off_and_clear);
  run_test("center and forward", test_center_and_forward);
  run_test("range", test_range);

  std::println("====================");
  std::println("Passed: {}/{}", passed, passed + failed);

  return failed > 0 ? 1 : 0;
}
