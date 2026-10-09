// Events, scene.mouse and the camera's mouse controls, against GlowScript's numbers for the same input
// (GlowScript run in Chrome on a 1280 x 657 canvas, center (0.5, 0.2, 0), range 3)

import vcpp;
import std;

using namespace vcpp;

namespace
{

bool near(const vec3& a, const vec3& b, double eps)
{ return std::abs(a.x() - b.x()) < eps && std::abs(a.y() - b.y()) < eps && std::abs(a.z() - b.z()) < eps; }

// A canvas set up as the GlowScript page was
void setup(canvas& c)
{
  g_viewport = {1280, 657};
  c.set_center(vec3{0.5, 0.2, 0});
  c.set_range(3);
  c.autoscale(1280, 657);
}

void drag(canvas& c, double x0, double y0, double x1, double y1, int which)
{
  mouse_down(c, x0, y0, which);
  for (int i = 1; i <= 5; ++i)
    mouse_move(c, x0 + (x1 - x0) * i / 5, y0 + (y1 - y0) * i / 5);
  mouse_up(c, x1, y1, which);
}

vec3 forward(const canvas& c) { return hat(c.m_camera.m_center - c.m_camera.m_pos); }

double range(const canvas& c)
{ return mag(c.m_camera.m_center - c.m_camera.m_pos) * std::tan(c.m_camera.m_fov * std::numbers::pi / 360.0); }

std::vector<std::string> g_seen;
void record(const event& ev) { g_seen.push_back(std::format("{} {:.5f} {:.5f}", ev.name(), ev.pos.x(), ev.pos.y())); }

// A press and release within 5 pixels is mousedown, mouseup, click; scene.mouse and project as GlowScript's
bool test_click_and_mouse()
{
  canvas c;
  setup(c);
  g_seen.clear();
  c.bind(event::mousedown | event::mouseup | event::click, record);
  mouse_move(c, 800, 400);
  const bool moved = near(c.mouse.pos, vec3{1.96119, -0.45297, 0}, 1e-5) &&
                     near(c.mouse.ray, vec3{0.26875, -0.12010, -0.95569}, 1e-5) && g_seen.empty();
  mouse_down(c, 700, 300, 1);
  mouse_up(c, 702, 301, 1);
  const std::vector<std::string> expected{"mousedown 1.04795 0.46027", "mouseup 1.06621 0.45114",
                                          "click 1.06621 0.45114"};
  const auto p = c.mouse.project(vec3{0, 0, 1}, vec3{0, 0, 1});
  return moved && g_seen == expected && p && near(*p, c.mouse.pos + c.mouse.ray * (1 / c.mouse.ray.z()), 1e-12);
}

// A left drag is mousedown, moves (delivered once per frame), mouseup, and no click
bool test_drag()
{
  canvas c;
  setup(c);
  g_seen.clear();
  c.bind(event::mousedown | event::mousemove | event::mouseup | event::click, record);
  mouse_down(c, 500, 200, 1);
  mouse_move(c, 530, 230);
  mouse_move(c, 560, 260);
  deliver_mouse_move(c);
  deliver_mouse_move(c); // nothing new
  mouse_up(c, 560, 260, 1);
  return g_seen == std::vector<std::string>{"mousedown -0.77854 1.37352", "mousemove -0.23059 0.82557",
                                            "mouseup -0.23059 0.82557"};
}

// Right-drag rotates, Shift-drag pans, Alt-drag zooms, Ctrl-drag rotates: GlowScript's camera after each
bool test_camera_controls()
{
  canvas c;
  setup(c);
  bool ok = true;
  drag(c, 600, 300, 660, 320, 3);
  ok = ok && near(forward(c), vec3{0.553387217, -0.198669331, -0.808883852}, 1e-9);
  c.mouse.shift = true;
  drag(c, 600, 300, 640, 330, 1);
  c.mouse.shift = false;
  ok = ok && near(c.m_camera.m_center, vec3{0.229241002, 0.468511391, -0.251185070}, 1e-9);
  c.mouse.alt = true;
  drag(c, 600, 300, 600, 280, 1);
  c.mouse.alt = false;
  ok = ok && std::abs(range(c) - 2.714512254) < 1e-9 && !c.m_autoscale;
  c.mouse.ctrl = true;
  drag(c, 300, 400, 250, 300, 1);
  c.mouse.ctrl = false;
  return ok && near(forward(c), vec3{0.069554611, 0.717356091, -0.693226078}, 1e-9);
}

int g_calls = 0;
void count() { ++g_calls; }

// Binding a function again adds types; unbind removes them; a handler may take nothing
bool test_bind_unbind()
{
  canvas c;
  g_calls = 0;
  c.bind(event::mousedown, count);
  c.bind(event::mouseup, count);
  c.trigger(event{.type = event::mousedown});
  c.trigger(event{.type = event::mouseup});
  c.unbind(event::mousedown, count);
  c.trigger(event{.type = event::mousedown});
  c.trigger(event{.type = event::mouseup});
  c.unbind(event::mouseup, count);
  c.trigger(event{.type = event::mouseup});
  return g_calls == 3;
}

// waitfor resumes at the next frame with the event; a coroutine handler can wait, and handlers run in order
bool test_waitfor_and_coroutine_handlers()
{
  canvas c;
  std::vector<std::string> order;
  std::string got;
  auto waiter = [&]() -> task<void> {
    const event ev = co_await c.waitfor(event::keydown);
    got = ev.key;
  };
  task<void> t = waiter();
  c.bind(event::click, [&](const event&) -> task<void> {
    order.push_back("first starts");
    co_await next_frame();
    order.push_back("first ends");
  });
  c.bind(event::click, [&] { order.push_back("second"); });
  c.trigger(event{.type = event::click});
  c.trigger(event{.type = event::keydown, .key = "p"});
  const bool before = got.empty() && order == std::vector<std::string>{"first starts"};
  tick_coroutines();
  tick_coroutines();
  return before && got == "p" && t.done() && order == std::vector<std::string>{"first starts", "first ends", "second"};
}

// GlowScript's key names, shifted while shift is held; keysdown lists the keys held
bool test_keys()
{
  canvas c;
  g_seen.clear();
  std::vector<std::string> keys;
  c.bind(event::keydown, [&](const event& ev) { keys.push_back(ev.key); });
  key_event(c, true, 65); // a
  key_event(c, true, 16); // shift
  key_event(c, true, 49); // 1, shifted: !
  key_event(c, false, 49);
  key_event(c, false, 16);
  key_event(c, true, 37); // left arrow
  const auto held = keysdown();
  key_event(c, false, 65);
  key_event(c, false, 37);
  return keys == std::vector<std::string>{"a", "shift", "!", "left"} && held == std::vector<std::string>{"a", "left"} &&
         keysdown().empty();
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
  run_test("click and scene.mouse", test_click_and_mouse);
  run_test("drag", test_drag);
  run_test("camera controls", test_camera_controls);
  run_test("bind and unbind", test_bind_unbind);
  run_test("waitfor and coroutine handlers", test_waitfor_and_coroutine_handlers);
  run_test("keys", test_keys);
  std::println("=================\nPassed: {}/{}", passed, passed + failed);
  return failed == 0 ? 0 : 1;
}
