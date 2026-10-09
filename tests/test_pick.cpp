// scene.mouse.pick() against GlowScript's pick of the same scene, sampled every 16 pixels of a 1280 x 657
// canvas (GlowScript run in Chrome: scratch page gsref/pickgrid.html). Letters name the objects in order; the
// curve's cells show its segment as a lower-case letter (b = segment 1). The box has no explicit up and the
// pyramid's axis is far from up: vcpp's constructors don't yet turn axis and up as GlowScript's do (known issue
// constructor-axis-up-rules), which would show here as a drawing difference, not a picking one.

import vcpp;
import std;

using namespace vcpp;

namespace
{

// clang-format off
const std::array<std::string_view, 41> glowscript{
"................................................................................",
"................................................................................",
"................................................................................",
"................................................................................",
"................................................................................",
"................................................................................",
"................................................................................",
"........................................................DDD.....................",
".......................AAA...................C.........DDDDD....................",
"......................AAAAA........BB.......CCC........DDDDD....................",
".....................AAAAAAA.....BBBBB.....CCCCC......DDDDD.....................",
".....................AAAAAAA...BBBBBB.....CCCCCCC.....DDDDD.....................",
"......................AAAAAA...BBBBB.....CCCCCCC.....DDDDD......................",
"......................AAAAAA..BBBBB.....CCCCCCC......DDDD.......................",
"........................AA...............CCCCC......DDDDD.......................",
"..........................................CC.........DDD........................",
"................................................................................",
"................................................................................",
"................................................................................",
"................................................................................",
"..........................E..............................ddddd..................",
".........................EEEEEE...FFFFF................dddd.....................",
".........................EEEEE...FFFFF.................c........................",
".........................EEE.....FFFF..........GGG.....c........................",
"..................................FF......GGGGGGG......cc.......................",
"..................................F....................cc.......................",
"........................................................c.......................",
"........................................................c.......................",
"............................J...........................cc......................",
"............................JJJ.........................cc......................",
"...............................JJ.........HH..........bbb.......................",
"...........................................HH........bb.........................",
".............................................H.....bbb..........................",
"..............................................HH...b............................",
"...............................................HH...............................",
"................................................................................",
"................................................................................",
"................................................................................",
"................................................................................",
"................................................................................",
"................................................................................"};
// clang-format on

bool test_pick_grid()
{
  canvas c;
  select(c);
  std::vector<object_ref> objs{sphere(pos = vec3{-4, 2, 0}, radius = 0.8),
                               ellipsoid(pos = vec3{-1.5, 2, 0}, size = vec3{2, 0.8, 1.2}, axis = vec3{1, 0.5, 0}),
                               box(pos = vec3{1, 2, 0}, size = vec3{1.5, 1, 0.6}, axis = vec3{1, 1, 0}),
                               cylinder(pos = vec3{3, 1.5, 0}, axis = vec3{0.5, 1.5, 0.3}, radius = 0.4),
                               cone(pos = vec3{-4, -1, 0}, axis = vec3{1.5, 0.5, 0}, radius = 0.6),
                               pyramid(pos = vec3{-1.5, -1, 0}, size = vec3{1.5, 1, 1}, axis = vec3{1, 0.6, 0}),
                               arrow(pos = vec3{0.5, -1, 0}, axis = vec3{2, 0.5, 0}),
                               arrow(pos = vec3{0.5, -2.5, 0}, axis = vec3{2, -0.5, 1}, round = true)};
  auto path = curve(radius = 0.15);
  path.append(std::vector<vec3>{vec3{3, -3, 0}, vec3{4, -1.5, 0.5}, vec3{3.5, 0, -0.5}, vec3{4.5, 0.5, 0}});
  objs.push_back(path);
  triangle_object t = build::triangle();
  t.m_v0.pos = vec3{-4, -3.5, 0};
  t.m_v1.pos = vec3{-2, -3.5, 0};
  t.m_v2.pos = vec3{-3, -2, 1};
  objs.push_back(c.add(t));
  select(scene);

  g_viewport = {1280, 657};
  c.set_center(vec3{0, 0, 0});
  c.set_forward(vec3{-0.3, -0.4, -1});
  c.set_range(5);
  c.autoscale(1280, 657);

  int differ = 0;
  for (std::size_t row = 0; row < glowscript.size(); ++row)
  {
    std::string line;
    for (int x = 8; x < 1280; x += 16)
    {
      mouse_move(c, x, 8 + 16 * static_cast<double>(row));
      const object_ref hit = c.mouse.pick();
      char cell = '.';
      for (std::size_t k = 0; k < objs.size(); ++k)
        if (hit && hit == objs[k])
          cell = k == 8 ? static_cast<char>('a' + *hit.segment) : static_cast<char>('A' + k);
      line += cell;
    }
    for (std::size_t i = 0; i < line.size(); ++i)
      differ += line[i] != glowscript[row][i];
    if (line != glowscript[row])
      std::println("  row {:2}: vcpp {}\n          gs   {}", row, line, glowscript[row]);
  }
  std::println("  {} of {} cells differ from GlowScript", differ, glowscript.size() * 80);
  return differ <= 12; // silhouette cells: GlowScript picks from tessellated meshes, vcpp from exact shapes
}

// What a pick gives: attributes applied by the handle's rules, comparison, empty results, pickable and visible
bool test_pick_result()
{
  canvas c;
  select(c);
  auto ball = sphere(pos = vec3{0, 0, 0}, radius = 1);
  auto wall = box(pos = vec3{0, 0, -3}, size = vec3{4, 4, 0.5});
  select(scene);
  g_viewport = {800, 600};
  c.set_center(vec3{0, 0, 0});
  c.set_range(3);
  c.autoscale(800, 600);
  mouse_move(c, 400, 300);
  auto hit = c.mouse.pick();
  const bool first = hit && hit == ball && !(hit == wall);
  hit.color = colors::red;
  const bool recoloured = ball.color == colors::red && hit.color == colors::red;
  ball.pickable = false;
  hit = c.mouse.pick();
  const bool behind = hit == wall;
  wall.visible = false;
  hit = c.mouse.pick();
  bool threw = false;
  try
  {
    hit.color = colors::blue;
  }
  catch (const std::logic_error&)
  {
    threw = true;
  }
  return first && recoloured && behind && !hit && threw && wall.color == colors::white;
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
      std::println("  ✗ {}", name);
      ++failed;
    }
  };
  run_test("pick grid", test_pick_grid);
  run_test("pick result", test_pick_result);
  std::println("=================\nPassed: {}/{}", passed, passed + failed);
  return failed == 0 ? 0 : 1;
}
