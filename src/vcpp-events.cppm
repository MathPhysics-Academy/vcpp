/*
 *  vcpp:events - GlowScript's events: what a handler receives, and scene.mouse
 *
 *  scene.bind(event::mousedown, on_down);              // a function taking const event&, or nothing
 *  scene.bind(event::mousemove | event::mouseup, on_drag);
 *  event ev = co_await scene.waitfor(event::click);
 *  vec3 p = scene.mouse.pos;
 *
 *  The canvas (vcpp:scene) keeps the bindings; vcpp:input turns the browser's mouse and keys into events.
 */

module;

import std;

export module vcpp:events;

import :vec;

export namespace vcpp
{

// Which events a handler or waitfor takes: event::mousedown, or several joined with |
struct event_types
{
  std::uint32_t bits{0};

  constexpr event_types operator|(event_types other) const noexcept { return {bits | other.bits}; }
  constexpr bool operator&(event_types other) const noexcept { return (bits & other.bits) != 0; }
  constexpr bool operator==(const event_types&) const noexcept = default;
};

// What GlowScript passes a handler. press and release are "left" or empty (GlowScript's None).
struct event
{
  static constexpr event_types mousedown{1};
  static constexpr event_types mouseup{2};
  static constexpr event_types mousemove{4};
  static constexpr event_types click{8};
  static constexpr event_types mouseenter{16};
  static constexpr event_types mouseleave{32};
  static constexpr event_types keydown{64};
  static constexpr event_types keyup{128};

  event_types type{};
  vec3 pos{};
  std::string press;
  std::string release;
  std::string key;
  int which{0};
  bool shift{false};
  bool ctrl{false};
  bool alt{false};

  // GlowScript's ev.event: the type's name
  std::string_view name() const noexcept
  {
    constexpr std::array names{"mousedown",  "mouseup",    "mousemove", "click",
                               "mouseenter", "mouseleave", "keydown",   "keyup"};
    for (std::size_t i = 0; i < names.size(); ++i)
      if (type.bits == (1u << i))
        return names[i];
    return "";
  }
};

// GlowScript's scene.mouse. pos is the mouse in the plane through scene.center facing the camera; ray points
// from the camera through it.
struct mouse_info
{
  vec3 pos{0, 0, 0};
  vec3 ray{0, 0, 1};
  bool shift{false};
  bool ctrl{false};
  bool alt{false};

  // Where the ray meets the plane with this normal through scene.center; empty if it never does
  std::optional<vec3> project(const vec3& normal) const { return project(normal, dot(normal, m_center)); }
  // ... the plane through point
  std::optional<vec3> project(const vec3& normal, const vec3& point) const
  { return project(normal, dot(normal, point)); }
  // ... the plane dot(normal, p) == d
  std::optional<vec3> project(const vec3& normal, double d) const
  {
    const double ndr = dot(normal, ray);
    if (ndr == 0)
      return std::nullopt;
    const double t = -(dot(normal, m_camera_pos) - d) / ndr;
    return m_camera_pos + ray * t;
  }

  // GlowScript's mouse.__update: (x, y) in pixels from the canvas's top left, on a canvas width x height;
  // the camera at camera_pos looking at center, with up, the view reaching tan_hfov * distance vertically
  void update(double x, double y, double width, double height, const vec3& camera_pos, const vec3& center,
              const vec3& up, double tan_hfov)
  {
    m_last = {x, y, width, height};
    const double factor = 2 * mag(center - camera_pos) * tan_hfov / height; // world units per pixel
    const double mx = (x - width / 2) * factor;
    const double my = (height - y - height / 2) * factor;
    const vec3 xaxis = hat(cross(hat(center - camera_pos), up));
    const vec3 yaxis = cross(xaxis, hat(center - camera_pos));
    pos = center + xaxis * mx + yaxis * my;
    ray = hat(pos - camera_pos);
    m_camera_pos = camera_pos;
    m_center = center;
  }

  // Recomputes pos and ray for a camera that moved, as GlowScript does when range, axis, center or up change;
  // nothing until the mouse has been over the canvas
  void refresh(const vec3& camera_pos, const vec3& center, const vec3& up, double tan_hfov)
  {
    if (m_last)
      update(m_last->x, m_last->y, m_last->width, m_last->height, camera_pos, center, up, tan_hfov);
  }

  // The size of the canvas the mouse was last seen on, in pixels; empty until then
  std::optional<std::pair<double, double>> canvas_size() const
  {
    if (!m_last)
      return std::nullopt;
    return std::pair{m_last->width, m_last->height};
  }

private:
  struct place
  {
    double x;
    double y;
    double width;
    double height;
  };
  std::optional<place> m_last; // where the mouse was last seen, and on what size of canvas
  vec3 m_camera_pos{0, 0, 10};
  vec3 m_center{0, 0, 0};
};

} // namespace vcpp
