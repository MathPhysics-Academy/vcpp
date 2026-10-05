/*
 *  test_handles.cpp - Handles from canvas::add stay valid as the scene grows
 */

import vcpp;
import std;

using namespace vcpp;

namespace
{

// Adding more spheres moves the vector; the handle still reaches the first one
bool test_survives_later_adds()
{
  canvas c;
  auto first = c.add(sphere(radius = 1.0));
  for (int i = 0; i < 100; ++i)
    c.add(sphere());
  first->m_radius = 2.0;
  return c.m_spheres.front().m_radius == 2.0;
}

// Other types added first don't shift which arrow a handle refers to
bool test_finds_its_own_object()
{
  canvas c;
  c.add(box());
  c.add(sphere());
  auto a = c.add(arrow());
  auto b = c.add(arrow());
  b->m_pos = vec3{1, 2, 3};
  return c.m_arrows[1].m_pos == vec3{1, 2, 3} && a->m_pos == vec3{0, 0, 0} && b.entry() == 3;
}

// After clear(), an old handle throws instead of reaching a different object
bool test_cleared_scene_throws()
{
  canvas c;
  auto old = c.add(sphere());
  c.clear();
  c.add(sphere());
  try
  {
    old->m_radius = 5.0;
  }
  catch (const std::logic_error&)
  {
    return c.m_spheres.front().m_radius == 1.0;
  }
  return false;
}

bool test_empty_handle()
{
  handle<sphere_object> h;
  try
  {
    (void)h->m_radius;
  }
  catch (const std::logic_error&)
  {
    return !h;
  }
  return false;
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

  std::println("vcpp handle tests");
  std::println("=================");

  run_test("survives later adds", test_survives_later_adds);
  run_test("finds its own object", test_finds_its_own_object);
  run_test("cleared scene throws", test_cleared_scene_throws);
  run_test("empty handle", test_empty_handle);

  std::println("=================");
  std::println("Passed: {}/{}", passed, passed + failed);

  return failed > 0 ? 1 : 0;
}
