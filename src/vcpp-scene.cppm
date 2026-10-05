/*
 *  vcpp:scene - Scene graph, camera, and lighting
 *
 *  Provides:
 *  - camera: view configuration
 *  - light: light source
 *  - canvas: scene container (like VPython's 'scene')
 */

module;

import std;

export module vcpp:scene;

import :vec;
import :color;
import :objects;
import :traits;

export namespace vcpp
{

// ============================================================================
// Camera
// ============================================================================

struct camera
{
  vec3 m_pos{0, 0, 10};   // camera position
  vec3 m_center{0, 0, 0}; // look-at point
  vec3 m_up{0, 1, 0};     // up direction
  double m_fov{60.0};     // field of view (degrees)
  double m_near{0.1};     // near clipping plane
  double m_far{1000.0};   // far clipping plane

  // Computed: forward direction
  constexpr vec3 forward() const noexcept { return hat(m_center - m_pos); }

  // Computed: right direction
  constexpr vec3 right() const noexcept { return hat(cross(forward(), m_up)); }

  // Orbit camera around center point
  void orbit(double horizontal_angle, double vertical_angle) noexcept
  {
    vec3 offset = m_pos - m_center;

    // Horizontal rotation (around up axis)
    offset = rotate(offset, horizontal_angle, m_up);

    // Vertical rotation (around right axis)
    vec3 r = hat(cross(forward(), m_up));
    offset = rotate(offset, vertical_angle, r);

    m_pos = m_center + offset;
  }

  // Zoom (move toward/away from center)
  void zoom(double factor) noexcept
  {
    vec3 offset = m_pos - m_center;
    double dist = mag(offset);
    double new_dist = dist * factor;
    if (new_dist < 0.1)
      new_dist = 0.1; // minimum distance
    m_pos = m_center + hat(offset) * new_dist;
  }

  // Pan (move camera and center together)
  void pan(const vec3& delta) noexcept
  {
    m_pos = m_pos + delta;
    m_center = m_center + delta;
  }
};

// ============================================================================
// Light
// ============================================================================

struct light
{
  vec3 m_pos{0, 10, 10};
  vec3 m_color{1, 1, 1};
  double m_intensity{1.0};
  bool m_directional{false}; // true = directional, false = point light

  // For directional lights, m_pos is treated as direction
  constexpr vec3 direction() const noexcept { return m_directional ? hat(m_pos) : vec3{0, 0, 0}; }
};

// ============================================================================
// Object Type Enum (for type-erased storage)
// ============================================================================

enum class object_type
{
  sphere,
  ellipsoid,
  box,
  cylinder,
  cone,
  arrow,
  ring,
  helix,
  pyramid,
  curve,
  points,
  label,
  triangle,
  quad,
  compound,
  text3d,
  extrusion
};

// ============================================================================
// Scene Entry (reference to an object in the scene)
// ============================================================================

struct scene_entry
{
  object_type type;
  std::size_t index; // index into type-specific storage
  bool dirty{true};
};

// ============================================================================
// handle<T> - An object in a canvas, by position rather than address
//
// The canvas keeps objects in per-type vectors, which move when they grow, so a reference into one
// dies as soon as another object of that type is added. A handle looks the object up on each use.
// Using a handle after its canvas has been cleared throws.
// ============================================================================

class canvas;

template<typename T>
class handle
{
public:
  handle() = default;

  T& operator*() const;
  T* operator->() const { return &**this; }

  // The object's index in the canvas's scene entries, as used to key trails
  std::size_t entry() const noexcept { return m_entry; }

  // Setters that apply GlowScript's rules for these attributes; writing the members directly doesn't
  void set_pos(const vec3& v) const;
  void set_axis(const vec3& v) const;
  void set_up(const vec3& v) const;
  void set_length(double length) const
    requires length_follows_axis<T>;
  void set_size(const vec3& size) const;

  explicit operator bool() const noexcept { return m_canvas != nullptr; }

private:
  friend class canvas;
  handle(canvas* c, std::size_t index, std::size_t entry, std::uint64_t generation) noexcept
    : m_canvas(c), m_index(index), m_entry(entry), m_generation(generation)
  {}

  canvas* m_canvas = nullptr;
  std::size_t m_index = 0;
  std::size_t m_entry = 0;
  std::uint64_t m_generation = 0;
};

// ============================================================================
// Canvas - The scene container
//
// Manages objects, camera, lights, and rendering state.
// Equivalent to VPython's 'scene' object.
// ============================================================================

class canvas
{
public:
  // ========== Canvas Properties ==========
  int m_width{800};
  int m_height{600};
  vec3 m_background{0, 0, 0};
  bool m_visible{true};
  bool m_resizable{true};
  std::string m_title{"VCpp"};
  std::string m_caption{};

  // ========== Camera ==========
  camera m_camera{};

  // ========== Lighting ==========
  std::vector<light> m_lights{
    light{{0, 10, 10}, {1, 1, 1}, 1.0, false} // default light
  };
  vec3 m_ambient{0.2, 0.2, 0.2};

  // ========== Object Storage (type-specific for cache efficiency) ==========
  std::vector<sphere_object> m_spheres;
  std::vector<ellipsoid_object> m_ellipsoids;
  std::vector<box_object> m_boxes;
  std::vector<cylinder_object> m_cylinders;
  std::vector<cone_object> m_cones;
  std::vector<arrow_object> m_arrows;
  std::vector<ring_object> m_rings;
  std::vector<helix_object> m_helixes;
  std::vector<pyramid_object> m_pyramids;
  std::vector<curve_object> m_curves;
  std::vector<points_object> m_points;
  std::vector<label_object> m_labels;
  std::vector<triangle_object> m_triangles;
  std::vector<quad_object> m_quads;
  std::vector<compound_object> m_compounds;
  std::vector<text3d_object> m_text3ds;
  std::vector<extrusion_object> m_extrusions;

  // ========== Trail Data ==========
  struct trail_data
  {
    std::vector<vec3> positions;
    int moves{0}; // set_pos calls since the last point, for interval
    vec3 color{1, 1, 1};
    double radius{0.02};
    mutable bool dirty{true};
  };
  std::unordered_map<std::size_t, trail_data> m_trails; // keyed by scene entry index

  // ========== Scene Graph ==========
  std::vector<scene_entry> m_entries;

  // ========== Dirty Tracking ==========
  bool m_scene_dirty{true};

  template<typename T>
  static constexpr object_type type_of()
  {
    if constexpr (std::same_as<T, sphere_object>)
      return object_type::sphere;
    else if constexpr (std::same_as<T, ellipsoid_object>)
      return object_type::ellipsoid;
    else if constexpr (std::same_as<T, box_object>)
      return object_type::box;
    else if constexpr (std::same_as<T, cylinder_object>)
      return object_type::cylinder;
    else if constexpr (std::same_as<T, cone_object>)
      return object_type::cone;
    else if constexpr (std::same_as<T, arrow_object>)
      return object_type::arrow;
    else if constexpr (std::same_as<T, ring_object>)
      return object_type::ring;
    else if constexpr (std::same_as<T, helix_object>)
      return object_type::helix;
    else if constexpr (std::same_as<T, pyramid_object>)
      return object_type::pyramid;
    else if constexpr (std::same_as<T, curve_object>)
      return object_type::curve;
    else if constexpr (std::same_as<T, points_object>)
      return object_type::points;
    else if constexpr (std::same_as<T, label_object>)
      return object_type::label;
    else if constexpr (std::same_as<T, triangle_object>)
      return object_type::triangle;
    else if constexpr (std::same_as<T, quad_object>)
      return object_type::quad;
    else if constexpr (std::same_as<T, compound_object>)
      return object_type::compound;
    else if constexpr (std::same_as<T, text3d_object>)
      return object_type::text3d;
    else
      return object_type::extrusion;
  }

private:
  std::uint64_t m_generation{0};

public:
  // ========== Object Registration ==========

  // Adds obj to the scene; the handle stays valid as more objects are added, until clear()
  template<typename T>
  handle<T> add(T obj)
  {
    auto& store = objects<T>();
    store.push_back(std::move(obj));
    m_entries.push_back({type_of<T>(), store.size() - 1, true});
    m_scene_dirty = true;
    return handle<T>(this, store.size() - 1, m_entries.size() - 1, m_generation);
  }

  // The storage for one object type
  template<typename T>
  std::vector<T>& objects()
  {
    if constexpr (std::same_as<T, sphere_object>)
      return m_spheres;
    else if constexpr (std::same_as<T, ellipsoid_object>)
      return m_ellipsoids;
    else if constexpr (std::same_as<T, box_object>)
      return m_boxes;
    else if constexpr (std::same_as<T, cylinder_object>)
      return m_cylinders;
    else if constexpr (std::same_as<T, cone_object>)
      return m_cones;
    else if constexpr (std::same_as<T, arrow_object>)
      return m_arrows;
    else if constexpr (std::same_as<T, ring_object>)
      return m_rings;
    else if constexpr (std::same_as<T, helix_object>)
      return m_helixes;
    else if constexpr (std::same_as<T, pyramid_object>)
      return m_pyramids;
    else if constexpr (std::same_as<T, curve_object>)
      return m_curves;
    else if constexpr (std::same_as<T, points_object>)
      return m_points;
    else if constexpr (std::same_as<T, label_object>)
      return m_labels;
    else if constexpr (std::same_as<T, triangle_object>)
      return m_triangles;
    else if constexpr (std::same_as<T, quad_object>)
      return m_quads;
    else if constexpr (std::same_as<T, compound_object>)
      return m_compounds;
    else if constexpr (std::same_as<T, text3d_object>)
      return m_text3ds;
    else
    {
      static_assert(std::same_as<T, extrusion_object>, "vcpp: not a scene object type");
      return m_extrusions;
    }
  }

  // Bumped by clear(), so handles from before it can be detected
  std::uint64_t generation() const noexcept { return m_generation; }

  // ========== Accessors ==========

  camera& cam() noexcept { return m_camera; }
  const camera& cam() const noexcept { return m_camera; }

  void background(const vec3& c) noexcept
  {
    m_background = c;
    m_scene_dirty = true;
  }
  vec3 background() const noexcept { return m_background; }

  // ========== Dirty Tracking ==========

  void mark_dirty() noexcept { m_scene_dirty = true; }
  bool is_dirty() const noexcept { return m_scene_dirty; }
  void clear_dirty() noexcept { m_scene_dirty = false; }

  // ========== Scene Info ==========

  std::size_t object_count() const noexcept { return m_entries.size(); }
  const std::vector<scene_entry>& entries() const noexcept { return m_entries; }

  // ========== Clear ==========

  void clear() noexcept
  {
    m_spheres.clear();
    m_ellipsoids.clear();
    m_boxes.clear();
    m_cylinders.clear();
    m_cones.clear();
    m_arrows.clear();
    m_rings.clear();
    m_helixes.clear();
    m_pyramids.clear();
    m_curves.clear();
    m_points.clear();
    m_labels.clear();
    m_triangles.clear();
    m_quads.clear();
    m_compounds.clear();
    m_text3ds.clear();
    m_extrusions.clear();
    m_trails.clear();
    m_entries.clear();
    ++m_generation;
    m_scene_dirty = true;
  }

  // ========== Trail Management ==========

  // Called once per render, as GlowScript's attach_trail is: a visible object that has moved since its
  // last trail point gets a new one, and only the newest m_retain points are kept.
  void update_trails()
  {
    // Iterate through all scene entries and update trails for objects with make_trail=true
    for (std::size_t entry_idx = 0; entry_idx < m_entries.size(); ++entry_idx)
    {
      const auto& entry = m_entries[entry_idx];
      const object_base* obj = nullptr;

      // Get the object based on type
      switch (entry.type)
      {
        case object_type::sphere:
          obj = &m_spheres[entry.index];
          break;
        case object_type::ellipsoid:
          obj = &m_ellipsoids[entry.index];
          break;
        case object_type::box:
          obj = &m_boxes[entry.index];
          break;
        case object_type::cylinder:
          obj = &m_cylinders[entry.index];
          break;
        case object_type::cone:
          obj = &m_cones[entry.index];
          break;
        case object_type::arrow:
          obj = &m_arrows[entry.index];
          break;
        case object_type::ring:
          obj = &m_rings[entry.index];
          break;
        case object_type::helix:
          obj = &m_helixes[entry.index];
          break;
        case object_type::pyramid:
          obj = &m_pyramids[entry.index];
          break;
        default:
          continue; // Skip non-trailable objects
      }

      // With an interval, set_pos adds the points instead
      if (!obj || !obj->m_make_trail || !obj->m_visible || obj->m_interval > 0)
        continue;

      auto& trail = m_trails[entry_idx];
      if (!trail.positions.empty() && trail.positions.back() == obj->m_pos)
        continue;
      add_trail_point(entry_idx, *obj);
    }
  }

  // Adds obj's position to the trail of scene entry `entry`, keeping the newest m_retain points
  void add_trail_point(std::size_t entry, const object_base& obj)
  {
    auto& trail = m_trails[entry];
    trail.color = obj.m_trail_color;
    trail.positions.push_back(obj.m_pos);
    if (obj.m_retain >= 0 && trail.positions.size() > static_cast<std::size_t>(obj.m_retain))
      trail.positions.erase(trail.positions.begin(), trail.positions.end() - static_cast<std::ptrdiff_t>(obj.m_retain));
    trail.dirty = true;
  }
};

template<typename T>
T& handle<T>::operator*() const
{
  if (!m_canvas || m_generation != m_canvas->generation())
    throw std::logic_error("vcpp: handle used after its scene was cleared, or never set");
  return m_canvas->objects<T>()[m_index];
}

namespace detail
{
// When one of axis and up turns from `from` to `to`, turn the other with it (GlowScript's adjust_up and
// adjust_axis in vectors.js)
inline void turn_with(vec3& other, const vec3& from, const vec3& to)
{
  if (dot(to, other) == 0)
    return; // already perpendicular
  const double angle = diff_angle(from, to);
  if (angle <= 1e-6)
    return;
  if (std::abs(angle - std::numbers::pi) < 1e-6)
    other = vec3{-other.x(), -other.y(), -other.z()}; // a 180-degree turn has no rotation axis
  else
    other = rotate(other, angle, cross(from, to));
}
} // namespace detail

// GlowScript's __update_trail: with an interval, every interval-th assignment adds a trail point,
// and the first assignment always does
template<typename T>
void handle<T>::set_pos(const vec3& v) const
{
  T& obj = **this;
  obj.m_pos = v;
  if (!obj.m_make_trail || !obj.m_visible || obj.m_interval <= 0)
    return;
  auto& trail = m_canvas->m_trails[m_entry];
  bool add = false;
  if (++trail.moves >= obj.m_interval)
  {
    trail.moves = 0;
    add = true;
  }
  else if (trail.moves == 1 && trail.positions.empty())
    add = true;
  if (add)
    m_canvas->add_trail_point(m_entry, obj);
}

// GlowScript's axis setter: up turns with the axis; for the box family the length follows; a zero
// axis is remembered and its predecessor used as the starting point once the axis is nonzero again
template<typename T>
void handle<T>::set_axis(const vec3& v) const
{
  T& obj = **this;
  vec3 from = obj.m_axis;
  obj.m_axis = v;
  if constexpr (length_follows_axis<T>)
    obj.m_length = mag(v);
  if (mag2(v) == 0)
  {
    if (!obj.m_axis_before_zero)
      obj.m_axis_before_zero = from;
    return;
  }
  if (obj.m_axis_before_zero)
  {
    from = *obj.m_axis_before_zero;
    obj.m_axis_before_zero.reset();
  }
  detail::turn_with(obj.m_up, from, v);
}

// GlowScript's up setter: the axis turns with up
template<typename T>
void handle<T>::set_up(const vec3& v) const
{
  T& obj = **this;
  const vec3 from = hat(obj.m_up);
  obj.m_up = v;
  detail::turn_with(obj.m_axis, from, v);
}

// GlowScript's length setter, including its quirk: after a zero length, a new length is set along
// (1,0,0), because it tests the old length after restoring the remembered axis
template<typename T>
void handle<T>::set_length(double length) const
  requires length_follows_axis<T>
{
  T& obj = **this;
  if (length == 0)
  {
    if (!obj.m_axis_before_zero)
      obj.m_axis_before_zero = obj.m_axis;
    obj.m_axis = vec3{0, 0, 0};
    obj.m_length = 0;
    return;
  }
  if (obj.m_axis_before_zero)
  {
    obj.m_axis = *obj.m_axis_before_zero;
    obj.m_axis_before_zero.reset();
  }
  if (obj.m_length == 0)
    set_axis(vec3{length, 0, 0});
  else
  {
    const vec3 dir = hat(obj.m_axis);
    set_axis(vec3{dir.x() * length, dir.y() * length, dir.z() * length});
  }
}

// GlowScript's size setter: the dimensions as size= sets them, then for the box family the axis
// keeps its direction and takes size.x as its length
template<typename T>
void handle<T>::set_size(const vec3& size) const
{
  T& obj = **this;
  if constexpr (std::same_as<T, box_object> || std::same_as<T, ellipsoid_object> || std::same_as<T, pyramid_object>)
    detail::set_size_lhw(obj, size);
  else if constexpr (std::same_as<T, cylinder_object> || std::same_as<T, cone_object> || std::same_as<T, helix_object>)
    detail::set_size_lr(obj, size);
  else
  {
    static_assert(std::same_as<T, sphere_object>, "vcpp: this object has no size");
    detail::set_size_sphere(obj, size);
  }
  if constexpr (length_follows_axis<T>)
  {
    vec3 dir = obj.m_axis;
    if (mag2(dir) == 0)
    {
      dir = obj.m_axis_before_zero.value_or(vec3{1, 0, 0});
      obj.m_axis_before_zero.reset();
    }
    dir = hat(dir);
    obj.m_axis = vec3{dir.x() * size.x(), dir.y() * size.x(), dir.z() * size.x()};
  }
}

// ============================================================================
// Global Default Scene (like VPython's 'scene')
// ============================================================================

inline canvas scene{};

// ============================================================================
// Canvas Selection (for multi-canvas support)
// ============================================================================

inline canvas* current_canvas = &scene;

inline void select(canvas& c) noexcept { current_canvas = &c; }

inline canvas& selected() noexcept { return *current_canvas; }

} // namespace vcpp
