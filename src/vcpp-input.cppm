/*
 *  vcpp:input - Input state and camera controls
 *
 *  Tracks mouse/keyboard state and provides camera interaction.
 *  Backend-agnostic: backends populate the state, this module processes it.
 */

module;

import std;

export module vcpp:input;

import :vec;
import :scene;

export namespace vcpp
{

// ============================================================================
// Mouse State
// ============================================================================

struct mouse_state
{
  double x{0}, y{0};           // current position (pixels)
  double last_x{0}, last_y{0}; // previous position
  bool left_down{false};
  bool right_down{false};
  bool middle_down{false};
  double scroll_delta{0}; // accumulated scroll since last frame
};

// ============================================================================
// Input State
// ============================================================================

struct input_state
{
  mouse_state mouse;
  bool shift_held{false};
  bool ctrl_held{false};
  bool alt_held{false};
  
  // Keyboard events (keys pressed this frame)
  std::vector<std::string> key_down_events;

  // Helper to check if a key was pressed and consume the event
  bool consume_key(const std::string& key)
  {
    auto it = std::find(key_down_events.begin(), key_down_events.end(), key);
    if (it != key_down_events.end())
    {
      key_down_events.erase(it);
      return true;
    }
    return false;
  }
};

// Global input state (populated by backend)
inline input_state g_input{};

// ============================================================================
// Input Processing Configuration
// ============================================================================

struct input_config
{
  double orbit_sensitivity{0.005};    // radians per pixel
  double pan_sensitivity{0.01};       // world units per pixel
  double zoom_sensitivity{0.1};       // zoom factor per scroll unit
  double zoom_drag_sensitivity{0.01}; // zoom factor per pixel (middle-drag)
};

inline input_config g_input_config{};

// ============================================================================
// Process Camera Input
//
// Call once per frame after input state is updated.
// Applies mouse input to camera controls.
// ============================================================================

inline void process_camera_input(canvas& c)
{
  auto& m = g_input.mouse;
  auto& cam = c.m_camera;

  double dx = m.x - m.last_x;
  double dy = m.y - m.last_y;

  // Left-drag: Orbit (Standard)
  // Condition: Left Down AND NOT Shift AND NOT Ctrl
  if (m.left_down && !g_input.ctrl_held && !g_input.shift_held && c.m_userspin)
  {
    if (dx != 0 || dy != 0)
    {
      cam.orbit(-dx * g_input_config.orbit_sensitivity, -dy * g_input_config.orbit_sensitivity);
      c.mark_dirty();
    }
  }

  // Panning: Shift + Left-drag (Trackpad Friendly) OR Ctrl + Left-drag
  // User explicitly disliked Right-Click for controls.
  bool is_panning = (m.left_down && g_input.shift_held) || (m.left_down && g_input.ctrl_held);
  
  if (is_panning)
  {
    if (dx != 0 || dy != 0)
    {
      vec3 r = cam.right();
      vec3 u = cam.m_up;
      double scale = g_input_config.pan_sensitivity;
      // Invert Y for intuitive "drag scene" feel? Or "move camera"?
      // Usually dragging scene: Mouse moves UP, Scene moves UP -> Camera moves DOWN.
      // Current: u * (dy * scale). If dy>0 (mouse down), camera moves UP?
      // Let's test standard CAD: Shift+Drag Up -> Pan Up.
      // This means camera moves DOWN.
      // If `cam.pan` moves camera position:
      // dy negative (mouse up) -> term is negative -> camera moves down -> scene up.
      // This matches "drag scene".
      c.m_autoscale = false; // as in GlowScript, zooming or panning ends autoscale
      cam.pan(r * (-dx * scale) + u * (dy * scale));
      c.mark_dirty();
    }
  }
  
  // Middle-drag: Pan or Zoom? Usually pan in Blender.
  // Let's make Middle-drag PAN as well to be safe for mouse users.
  if (m.middle_down) {
    if (dx != 0 || dy != 0)
    {
       vec3 r = cam.right();
       vec3 u = cam.m_up;
       double scale = g_input_config.pan_sensitivity;
       c.m_autoscale = false;
       cam.pan(r * (-dx * scale) + u * (dy * scale));
       c.mark_dirty();
    }
  }

  // Scroll wheel: zoom
  // Trackpads generate scroll events.
  if (m.scroll_delta != 0 && c.m_userzoom)
  {
    // Reduce sensitivity for trackpads (which generate high delta)
    double sensitivity = g_input_config.zoom_sensitivity;
    // Cap delta per frame to avoid jumping?
    // Or just use factor.
    double factor = 1.0 - m.scroll_delta * sensitivity;
    if (factor > 0.01 && factor < 100.0) 
    {
      c.m_autoscale = false;
      cam.zoom(factor);
      c.mark_dirty();
    }
  }
  m.scroll_delta = 0; // consume scroll

  // Update last position for next frame
  m.last_x = m.x;
  m.last_y = m.y;
}

// ============================================================================
// GlowScript's mouse and keys (orbital_camera.js, canvas.js)
//
// The left button belongs to the program: its press, release, drag and click become events. The camera moves
// with the others: right-drag or Ctrl-drag rotates, middle-drag, Alt-drag or both buttons zoom, Shift-drag
// pans, the wheel zooms. A drag's moves reach the program once per frame (deliver_mouse_move); without a
// button down, moving the mouse only updates scene.mouse. Backends call these with positions in pixels from
// the canvas's top left, and buttons numbered as GlowScript's `which`: 1 left, 2 middle, 3 right.
// ============================================================================

// The canvas's size in the pixels mouse positions are measured in; backends keep it current
struct viewport
{
  double width{800};
  double height{600};
};
inline viewport g_viewport{};

namespace detail
{
struct pointer_state
{
  bool left{false};
  bool right{false};
  bool rotating{false};
  bool zooming{false};
  bool panning{false};
  bool afterdown{false}; // a press was seen, so moves and the release count
  double down_x{0};
  double down_y{0};
  double last_x{0};
  double last_y{0};
  vec3 lastpos{};
  std::optional<std::pair<double, double>> pending_move;
};
inline pointer_state g_pointer{};

inline void update_mouse(canvas& c, double x, double y)
{
  const camera& cam = c.m_camera;
  c.mouse.update(x, y, g_viewport.width, g_viewport.height, cam.m_pos, cam.m_center, cam.m_up,
                 std::tan(cam.m_fov * std::numbers::pi / 360.0));
}

inline event mouse_event(canvas& c, event_types type, double x, double y)
{
  update_mouse(c, x, y);
  event ev;
  ev.type = type;
  ev.pos = c.mouse.pos;
  ev.which = 1;
  ev.shift = c.mouse.shift;
  ev.ctrl = c.mouse.ctrl;
  ev.alt = c.mouse.alt;
  if (type == event::mousedown)
    ev.press = "left";
  else if (type == event::mouseup || type == event::click)
    ev.release = "left";
  return ev;
}

// GlowScript's zoom: range times exp(-0.05 delta), which ends autoscale
inline void zoom(canvas& c, double delta)
{
  camera& cam = c.m_camera;
  cam.m_pos = cam.m_center + (cam.m_pos - cam.m_center) * std::exp(-delta * 0.05);
  c.m_autoscale = false;
  c.mark_dirty();
  c.refresh_mouse();
}

// GlowScript's spin: 0.01 radian per pixel about up, and about axis x up unless that would pass over the top
inline void spin(canvas& c, double dx, double dy)
{
  camera& cam = c.m_camera;
  vec3 axis = rotate(cam.m_center - cam.m_pos, -0.01 * dx, cam.m_up);
  const double max_vertical = diff_angle(cam.m_up, -axis);
  const double vertical = 0.01 * dy;
  if (!(vertical >= max_vertical || vertical <= max_vertical - std::numbers::pi))
    axis = rotate(axis, -vertical, cross(axis, cam.m_up));
  cam.m_pos = cam.m_center - axis;
  c.mark_dirty();
  c.refresh_mouse();
}

// GlowScript's pan: the point grabbed stays under the mouse
inline void pan(canvas& c, double x, double y)
{
  update_mouse(c, x, y);
  const camera& cam = c.m_camera;
  const vec3 xaxis = hat(cross(cam.m_center - cam.m_pos, cam.m_up));
  const vec3 yaxis = hat(cross(xaxis, cam.m_center - cam.m_pos));
  const vec3 d = c.mouse.pos - g_pointer.lastpos;
  c.set_center(cam.m_center - (xaxis * dot(d, xaxis) + yaxis * dot(d, yaxis)));
}

// GlowScript's names for key codes, as typed without and with shift
// clang-format off
inline constexpr std::array<std::string_view, 128> unshifted_keys{
    "", "", "", "", "", "", "", "", "backspace", "tab",
    "", "", "", "\n", "", "", "shift", "ctrl", "alt", "",
    "caps lock", "", "", "", "", "", "", "esc", "", "",
    "", "", " ", "pageup", "pagedown", "end", "home", "left", "up", "right",
    "down", "", "", "", ",", "insert", "delete", "/", "0", "1",
    "2", "3", "4", "5", "6", "7", "8", "9", "", ";",
    "", "=", "", "", "", "a", "b", "c", "d", "e",
    "f", "g", "h", "i", "j", "k", "l", "m", "n", "o",
    "p", "q", "r", "s", "t", "u", "v", "w", "x", "y",
    "z", "[", "\\", "]", "", "", "`", "", "", "",
    "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "delete",
};
inline constexpr std::array<std::string_view, 128> shifted_keys{
    "", "", "", "", "", "", "", "", "backspace", "tab",
    "", "", "", "\n", "", "", "shift", "ctrl", "alt", "break",
    "caps lock", "", "", "", "", "", "", "esc", "", "",
    "", "", "", "!", "\"", "//", "$", "%", "&", "\"",
    "(", ")", "*", "+", "<", "_", ">", "?", ")", "!",
    "@", "#", "$", "%", "^", "&", "*", "(", ":", ":",
    "<", "=", ">", "?", "@", "A", "B", "C", "D", "E",
    "F", "G", "H", "I", "J", "K", "L", "M", "N", "O",
    "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y",
    "Z", "{", "|", "}", "^", "_", "~", "", "", "",
    "", "", "", "", "", "", "*", "+", "", "",
    "", "", "f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8",
    "f9", "f10", "", "{", "|", "}", "~", "delete",
};
// clang-format on

inline std::string_view key_name(int which, bool shifted)
{
  if (which >= 0 && which < 128)
    return shifted ? shifted_keys[static_cast<std::size_t>(which)] : unshifted_keys[static_cast<std::size_t>(which)];
  static constexpr std::array<std::pair<int, std::pair<std::string_view, std::string_view>>, 11> punctuation{
    {{187, {"=", "+"}},
     {189, {"-", "_"}},
     {192, {"`", "~"}},
     {219, {"[", "{"}},
     {220, {"\\", "|"}},
     {221, {"]", "}"}},
     {186, {";", ":"}},
     {222, {"'", "\""}},
     {188, {",", "<"}},
     {190, {".", ">"}},
     {191, {"/", "?"}}}};
  for (const auto& [code, names] : punctuation)
    if (code == which)
      return shifted ? names.second : names.first;
  return "";
}

inline std::vector<std::string> g_keys_down;
inline bool g_shiftlock = false;
} // namespace detail

inline void mouse_down(canvas& c, double x, double y, int which)
{
  auto& p = detail::g_pointer;
  if (which == 1)
    p.left = true;
  if (which == 3)
    p.right = true;
  p.rotating = c.m_userspin && (which == 3 || (which == 1 && c.mouse.ctrl && !c.mouse.alt));
  p.zooming = c.m_userzoom && (which == 2 || (which == 1 && c.mouse.alt && !c.mouse.ctrl) || (p.left && p.right));
  p.panning = which == 1 && c.mouse.shift;
  if (which == 3 && !(p.rotating || p.zooming))
    return;
  p.down_x = p.last_x = x;
  p.down_y = p.last_y = y;
  if (!(p.rotating || p.zooming || p.panning) && which == 1)
    c.trigger(detail::mouse_event(c, event::mousedown, x, y));
  if (p.panning)
  {
    c.m_autoscale = false;
    detail::update_mouse(c, x, y);
    p.lastpos = c.mouse.pos;
  }
  p.afterdown = true;
}

inline void mouse_move(canvas& c, double x, double y)
{
  auto& p = detail::g_pointer;
  if (x == p.last_x && y == p.last_y)
    return;
  if (!p.afterdown)
  {
    detail::update_mouse(c, x, y);
    return;
  }
  if (p.zooming)
  {
    const double dy = p.last_y - y;
    if (dy != 0)
      detail::zoom(c, 0.1 * dy);
  }
  else if (p.rotating)
    detail::spin(c, x - p.last_x, y - p.last_y);
  else if (p.panning)
    detail::pan(c, x, y);
  else if (p.left)
    p.pending_move = {x, y};
  if (!p.panning)
  {
    p.last_x = x;
    p.last_y = y;
  }
}

inline void mouse_up(canvas& c, double x, double y, int which)
{
  auto& p = detail::g_pointer;
  if (which == 1)
    p.left = false;
  if (which == 3)
    p.right = false;
  if (!p.afterdown)
    return;
  if (!(p.rotating || p.zooming || p.panning))
  {
    if (which == 1)
    {
      c.trigger(detail::mouse_event(c, event::mouseup, x, y));
      if (std::abs(x - p.down_x) <= 5 && std::abs(y - p.down_y) <= 5)
        c.trigger(detail::mouse_event(c, event::click, x, y));
    }
    else if (which == 3)
      return;
  }
  p.rotating = p.zooming = p.panning = p.afterdown = false;
}

// GlowScript's wheel zooms by its delta, about one per notch
inline void mouse_wheel(canvas& c, double delta)
{
  if (c.m_userzoom)
    detail::zoom(c, delta);
}

inline void mouse_enter(canvas& c, double x, double y, bool entering)
{ c.trigger(detail::mouse_event(c, entering ? event::mouseenter : event::mouseleave, x, y)); }

// Once per frame: a left-drag's latest move goes to the program, as GlowScript sends it at render time
inline void deliver_mouse_move(canvas& c)
{
  if (const auto move = std::exchange(detail::g_pointer.pending_move, std::nullopt))
    c.trigger(detail::mouse_event(c, event::mousemove, move->first, move->second));
}

// A key pressed or released; which is the browser's key code
inline void key_event(canvas& c, bool down, int which)
{
  if (which == 16)
    c.mouse.shift = down;
  if (which == 17)
    c.mouse.ctrl = down;
  if (which == 18)
    c.mouse.alt = down;
  event ev;
  ev.type = down ? event::keydown : event::keyup;
  ev.which = which;
  ev.shift = c.mouse.shift || detail::g_shiftlock;
  const bool letter = which >= 65 && which <= 90;
  ev.key = detail::key_name(which, (detail::g_shiftlock && letter) || c.mouse.shift);
  ev.ctrl = c.mouse.ctrl;
  ev.alt = c.mouse.alt;
  auto& held = detail::g_keys_down;
  const auto it = std::ranges::find(held, ev.key);
  if (down && it == held.end())
    held.push_back(ev.key);
  else if (!down && it != held.end())
    held.erase(it);
  if (!c.expects_keys())
    return;
  if (which == 20 && down)
    detail::g_shiftlock = !detail::g_shiftlock;
  c.trigger(ev);
}

// GlowScript's keysdown(): the keys held now
inline std::vector<std::string> keysdown() { return detail::g_keys_down; }

} // namespace vcpp
