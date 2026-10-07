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

bool near(const vec3& a, const vec3& b) { return mag(vec3{a.x() - b.x(), a.y() - b.y(), a.z() - b.z()}) < 1e-9; }

// With an interval, the first set_pos and then every interval-th one add a trail point
bool test_interval_counts_moves()
{
  canvas c;
  auto ball = c.add(sphere(make_trail = true, interval = 3));
  for (int i = 1; i <= 7; ++i)
    ball.set_pos(vec3{static_cast<double>(i), 0, 0});
  c.update_trails(); // with an interval, the per-render step adds nothing
  const auto& points = c.m_trails[ball.entry()].positions;
  return points.size() == 3 && points[0] == vec3{1, 0, 0} && points[1] == vec3{3, 0, 0} && points[2] == vec3{6, 0, 0};
}

// Turning axis turns up with it, so a cylinder pointed straight down keeps a valid up
bool test_up_turns_with_axis()
{
  canvas c;
  auto leg = c.add(cylinder());
  leg.set_axis(vec3{0, -1.2, 0});
  return near(leg->m_up, vec3{1, 0, 0}) && leg->m_length == 1.2;
}

// A 180-degree turn has no rotation axis, so up flips; but an up already perpendicular to the new
// axis is left alone, as GlowScript's adjust_up returns early
bool test_axis_flip()
{
  canvas c;
  auto tilted = c.add(box(axis = vec3{1, 1, 0}));
  tilted.set_axis(vec3{-1, -1, 0});
  auto level = c.add(box());
  level.set_axis(vec3{-2, 0, 0});
  return near(tilted->m_up, vec3{0, -1, 0}) && near(level->m_up, vec3{0, 1, 0});
}

// Setting up turns axis the same way
bool test_axis_turns_with_up()
{
  canvas c;
  auto b = c.add(box());
  b.set_up(vec3{-1, 0, 0});
  return near(b->m_axis, vec3{0, 1, 0});
}

// length rescales axis, keeping its direction; a zero length remembers the axis
bool test_length_and_axis()
{
  canvas c;
  auto b = c.add(box(axis = vec3{0, 0, 2}));
  b.set_length(5);
  bool scaled = near(b->m_axis, vec3{0, 0, 5}) && b->m_length == 5;
  b.set_axis(vec3{0, 0, 0});
  b.set_axis(vec3{0, 3, 0}); // turns up from the remembered (0,0,5), not from zero
  return scaled && b->m_length == 3 && near(b->m_up, vec3{0, 0, -1});
}

// set_size sets the dimensions and gives axis the length size.x
bool test_set_size()
{
  canvas c;
  auto cyl = c.add(cylinder(axis = vec3{0, 2, 0}));
  cyl.set_size(vec3{4, 1, 1});
  return near(cyl->m_axis, vec3{0, 4, 0}) && cyl->m_length == 4 && cyl->m_radius == 0.5;
}

// rotate turns axis and up together, and pos about an origin; the expected values are GlowScript's. An arrow
// along up starts from GlowScript's up for it, (-1, 0, 0), not the (0, 1, 0) it was given.
bool test_rotate()
{
  canvas c;
  auto moved = c.add(box(pos = vec3{1, 0, 0}));
  moved.rotate(std::numbers::pi / 2, vec3{0, 0, 1}, vec3{0, 0, 0});
  auto spun = c.add(box());
  spun.rotate(0.3);
  auto upright = c.add(arrow(axis = vec3{0, 2, 0}));
  upright.rotate(0.5, vec3{1, 0, 0});
  return near(moved->m_pos, vec3{0, 1, 0}) && near(moved->m_axis, vec3{0, 1, 0}) && near(moved->m_up, vec3{-1, 0, 0}) &&
         near(spun->m_axis, vec3{1, 0, 0}) && near(spun->m_up, vec3{0, std::cos(0.3), std::sin(0.3)}) &&
         near(upright->m_axis, vec3{0, 1.7551651237807455, 0.958851077208406}) && near(upright->m_up, vec3{-1, 0, 0});
}

// Attributes read and write the object as in VPython. pos keeps GlowScript's rule (a trail point per
// move, with interval 1); copies name the same object; after clear() an attribute throws like the handle.
bool test_attributes()
{
  canvas c;
  auto ball = c.add(sphere(pos = vec3{0, 4, 0}, radius = 0.5, make_trail = true, interval = 1));
  const vec3 v{1, 0, 0};
  const double dt = 0.5;
  ball.pos = ball.pos + v * dt;
  ball.pos += v;
  auto same = ball;
  same.color = colors::red;
  ball.radius = ball.radius * 2;
  std::vector<handle<sphere_object>> balls{ball};
  balls[0].opacity = 0.5;
  auto other = c.add(sphere(pos = vec3{9, 9, 9}));
  other = ball;
  other.visible = false;
  const bool before = near(ball.pos.value(), vec3{1.5, 4, 0}) && ball.pos.y() == 4 && mag(ball.pos - v) > 0 &&
                      near(ball->m_color, colors::red) && ball->m_radius == 1 && ball->m_opacity == 0.5 &&
                      !ball->m_visible && c.m_trails[ball.entry()].positions.size() == 2;
  c.clear();
  try
  {
    ball.pos = vec3{0, 0, 0};
  }
  catch (const std::logic_error&)
  {
    return before;
  }
  return false;
}

// The attributes beyond pos: sizes and lengths go through GlowScript's rules, the rest are written as they are
bool test_more_attributes()
{
  canvas c;
  auto wall = c.add(box(axis = vec3{0, 2, 0}));
  wall.length = 4;
  const bool box_ok = near(wall.axis.value(), vec3{0, 4, 0}) && wall.size == vec3{4, 1, 1};
  wall.size = vec3{1, 2, 3};
  wall.width += 1;

  auto rod = c.add(cylinder());
  rod.size = vec3{3, 1, 1};
  auto pointer = c.add(arrow(axis = vec3{0, 0, 2}));
  pointer.length = 1;
  auto ball = c.add(sphere());
  ball.radius = 2;
  ball.texture = textures::earth;
  ball.make_trail = true;
  ball.trail_color = colors::yellow;
  ball = ball;
  auto tag = c.add(label(text = "t = 0"));
  tag.text = std::format("t = {}", 1.5);
  auto path = c.add(curve());
  path->m_geometry_dirty = false;
  path.radius = 0.2;

  return box_ok && wall.size == vec3{1, 2, 4} && near(wall.axis.value(), vec3{0, 1, 0}) && rod.radius == 0.5 &&
         rod.length == 3 && near(pointer.axis.value(), vec3{0, 0, 1}) && pointer.length == 1 &&
         ball.size == vec3{4, 4, 4} && ball.texture.value() == textures::earth && ball->m_make_trail &&
         ball->m_trail_color == colors::yellow && tag->m_text == "t = 1.5" && path->m_geometry_dirty;
}

// GlowScript's curve methods. A point's own colour and radius win over the curve's; (-1, -1, -1) and 0 mean unset.
bool test_curve_points()
{
  canvas c;
  auto square = c.add(curve(color = colors::yellow, radius = 0.05));
  square.append(vec3{0, 0, 0});
  square.append(pos = vec3{0, 1, 0}, color = colors::cyan, radius = 0.1);
  square.append(std::vector<vec3>{vec3{1, 1, 0}, vec3{1, 0, 0}});
  const curve_point second = square.point(1);
  const curve_point last = square.point(-1);
  square.modify(1, color = colors::red);
  square.modify(-1, vec3{2, 0, 0});
  bool threw = false;
  try
  {
    square.point(4);
  }
  catch (const std::out_of_range&)
  {
    threw = true;
  }
  const bool points_ok = square.npoints == 4 && second.pos == vec3{0, 1, 0} && second.color == colors::cyan &&
                         second.radius == 0.1 && last.color == vec3{-1, -1, -1} && last.radius == 0 &&
                         square.point(1).color == colors::red && square.point(1).radius == 0.1 &&
                         square.point(3).pos == vec3{2, 0, 0} && threw;
  square->m_geometry_dirty = false;
  square.clear();
  return points_ok && square.npoints == 0 && square->m_geometry_dirty;
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
  run_test("interval counts moves", test_interval_counts_moves);
  run_test("up turns with axis", test_up_turns_with_axis);
  run_test("axis flip", test_axis_flip);
  run_test("axis turns with up", test_axis_turns_with_up);
  run_test("length and axis", test_length_and_axis);
  run_test("set_size", test_set_size);
  run_test("rotate", test_rotate);
  run_test("attributes", test_attributes);
  run_test("more attributes", test_more_attributes);
  run_test("curve points", test_curve_points);

  std::println("=================");
  std::println("Passed: {}/{}", passed, passed + failed);

  return failed > 0 ? 1 : 0;
}
