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

  // Clipping planes scale with the camera's distance from center, as in GlowScript, so a scene in metres
  // and one in astronomical units both show
  double near_plane() const noexcept { return mag(m_pos - m_center) / 100; }
  double far_plane() const noexcept { return mag(m_pos - m_center) * 10; }

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
  extrusion,
  distant_light,
  local_light
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
// handle_base<T> - An object in a canvas, by position rather than address
//
// The canvas keeps objects in per-type vectors, which move when they grow, so a reference into one
// dies as soon as another object of that type is added. A handle looks the object up on each use.
// Using a handle after its canvas has been cleared throws. handle<T>, below, adds the attributes.
// ============================================================================

class canvas;

template<typename T>
class handle;

template<typename T>
class handle_base
{
public:
  handle_base() = default;

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

  // GlowScript's rotate: axis and up turn together by angle about rotation_axis, the object's own axis by
  // default; given an origin other than pos, pos turns about it too
  void rotate(double angle, std::optional<vec3> rotation_axis = {}, std::optional<vec3> origin = {}) const;

  // A curve's methods, as GlowScript names them: append, point, modify, clear
  template<typename... Args>
  void append(Args&&... args) const
    requires std::same_as<T, curve_object>
  { (**this).append(std::forward<Args>(args)...); }
  void append(const std::vector<vec3>& points) const
    requires std::same_as<T, curve_object>
  { (**this).append(points); }
  curve_point point(std::ptrdiff_t n) const
    requires std::same_as<T, curve_object>
  { return (**this).point(n); }
  template<typename... Args>
  void modify(std::ptrdiff_t n, Args&&... args) const
    requires std::same_as<T, curve_object>
  { (**this).modify(n, std::forward<Args>(args)...); }
  void clear() const
    requires std::same_as<T, curve_object>
  { (**this).clear(); }

  explicit operator bool() const noexcept { return m_canvas != nullptr; }

private:
  friend class canvas;
  handle_base(canvas* c, std::size_t index, std::size_t entry, std::uint64_t generation) noexcept
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
  std::string m_title{};   // HTML shown above the canvas, as GlowScript's scene.title
  std::string m_caption{}; // and below it, as scene.caption

  // ========== Camera ==========
  camera m_camera{};

  // ========== Lighting ==========
  // GlowScript's: every canvas starts with two distant lights and an ambient light of gray 0.2
  static std::vector<distant_light_object> default_lights()
  {
    return {distant_light_object{{0.22, 0.44, 0.88}, {0.8, 0.8, 0.8}},
            distant_light_object{{-0.88, -0.22, -0.44}, {0.3, 0.3, 0.3}}};
  }
  std::vector<distant_light_object> m_distant_lights = default_lights();
  std::vector<local_light_object> m_local_lights;
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
    std::uint64_t added{0}; // points ever added; the renderer compares it to find the new ones
    int retain{-1};
    int moves{0}; // set_pos calls since the last point, for interval
    vec3 color{1, 1, 1};
    double radius{0}; // 0: a thin line a few pixels wide, as in GlowScript
    bool points{false}; // a sphere at each point instead of a curve
  };
  std::unordered_map<std::size_t, trail_data> m_trails; // keyed by scene entry index

  // ========== Scene Graph ==========
  std::vector<scene_entry> m_entries;

  // ========== Dirty Tracking ==========
  bool m_scene_dirty{true};

  // GlowScript's scene.autoscale: move the camera out to keep everything in view. Turned off by the
  // user zooming or panning, or by the program setting it false.
  bool m_autoscale{true};

  // GlowScript's scene.userspin and scene.userzoom: whether the user may rotate or zoom the view
  bool m_userspin{true};
  bool m_userzoom{true};

  void append_to_title(std::string_view html) { m_title += html; }
  void append_to_caption(std::string_view html) { m_caption += html; }

  // GlowScript's scene.center: the camera keeps its direction and distance from the new point
  void set_center(const vec3& center)
  {
    m_camera.m_pos = center + (m_camera.m_pos - m_camera.m_center);
    m_camera.m_center = center;
    m_scene_dirty = true;
  }

  // GlowScript's scene.forward: the camera looks along `forward`, at the same distance from center
  void set_forward(const vec3& forward)
  {
    m_camera.m_pos = m_camera.m_center - hat(forward) * mag(m_camera.m_pos - m_camera.m_center);
    m_scene_dirty = true;
  }

  // GlowScript's scene.range: how far from center the view reaches, along the canvas's shorter side.
  // It ends autoscale. The camera's distance depends on the canvas's shape, so it is set at the next render.
  void set_range(double range)
  {
    m_range = range;
    m_autoscale = false;
    m_scene_dirty = true;
  }

  // GlowScript's scene.fov, in radians
  void set_fov(double fov)
  {
    m_camera.m_fov = fov * 180 / std::numbers::pi;
    m_scene_dirty = true;
  }

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
    else if constexpr (std::same_as<T, distant_light_object>)
      return object_type::distant_light;
    else if constexpr (std::same_as<T, local_light_object>)
      return object_type::local_light;
    else
      return object_type::extrusion;
  }

private:
  std::uint64_t m_generation{0};
  double m_range{0}; // a scene.range not yet applied
  double m_autoscale_last_zx{-1};
  double m_autoscale_last_zy{-1};

  // An axis-aligned bounding box in world coordinates
  struct extent
  {
    vec3 lo;
    vec3 hi;
    bool empty{true};

    void add(const vec3& p)
    {
      if (empty)
      {
        lo = hi = p;
        empty = false;
        return;
      }
      lo = vec3{std::min(lo.x(), p.x()), std::min(lo.y(), p.y()), std::min(lo.z(), p.z())};
      hi = vec3{std::max(hi.x(), p.x()), std::max(hi.y(), p.y()), std::max(hi.z(), p.z())};
    }

    // The 8 corners of a box with this center, orientation and size
    void add_box(const vec3& center, const orientation& o, double length, double height, double width)
    {
      for (double sx : {-0.5, 0.5})
        for (double sy : {-0.5, 0.5})
          for (double sz : {-0.5, 0.5})
            add(center + o.x * (sx * length) + o.y * (sy * height) + o.z * (sz * width));
    }

    // A box that starts at pos and runs along axis, as cylinders, cones, pyramids and arrows do
    void add_box_from_base(const object_base& obj, double length, double height, double width)
    {
      const auto o = orientation_of(obj.m_axis, obj.m_up);
      add_box(obj.m_pos + o.x * (length / 2), o, length, height, width);
    }

    void add_ball(const vec3& center, double radius)
    {
      add(center - vec3{radius, radius, radius});
      add(center + vec3{radius, radius, radius});
    }
  };

  // Calls f with the extent of each visible object that autoscale counts. Labels are 2D overlays and
  // 3D text is left out, as GlowScript leaves it out.
  template<typename F>
  void for_each_extent(F&& f) const
  {
    auto each = [&](const auto& objects, auto&& fill) {
      for (const auto& obj : objects)
      {
        if (!obj.m_visible)
          continue;
        extent e;
        fill(e, obj);
        if (!e.empty)
          f(e);
      }
    };
    auto centered = [](extent& e, const auto& obj) {
      e.add_box(obj.m_pos, orientation_of(obj.m_axis, obj.m_up), obj.m_length, obj.m_height, obj.m_width);
    };
    auto round = [](extent& e, const auto& obj) {
      e.add_box_from_base(obj, mag(obj.m_axis), 2 * obj.m_radius, 2 * obj.m_radius);
    };
    each(m_spheres, [](extent& e, const sphere_object& s) { e.add_ball(s.m_pos, s.m_radius); });
    each(m_boxes, centered);
    each(m_ellipsoids, centered);
    each(m_cylinders, round);
    each(m_cones, round);
    each(m_helixes, round);
    each(m_pyramids,
         [](extent& e, const pyramid_object& p) { e.add_box_from_base(p, p.m_length, p.m_height, p.m_width); });
    each(m_arrows, [](extent& e, const arrow_object& a) {
      const double w = std::max(a.m_shaftwidth, a.m_headwidth);
      e.add_box_from_base(a, mag(a.m_axis), w, w);
    });
    each(m_rings, [](extent& e, const ring_object& r) {
      const double d = 2 * (r.m_radius + r.m_thickness);
      e.add_box(r.m_pos, orientation_of(r.m_axis, r.m_up), 2 * r.m_thickness, d, d);
    });
    each(m_curves, [](extent& e, const curve_object& c) {
      for (const curve_point& p : c.m_points)
        e.add_ball(p.pos, p.radius > 0 ? p.radius : c.m_radius);
    });
    each(m_points, [](extent& e, const points_object& pts) {
      for (const auto& p : pts.m_points)
        e.add(p);
    });
    each(m_triangles, [](extent& e, const triangle_object& t) {
      for (const auto* v : {&t.m_v0, &t.m_v1, &t.m_v2})
        e.add(v->pos);
    });
    each(m_quads, [](extent& e, const quad_object& q) {
      for (const auto* v : {&q.m_v0, &q.m_v1, &q.m_v2, &q.m_v3})
        e.add(v->pos);
    });
    each(m_compounds, [](extent& e, const compound_object& c) {
      const auto o = orientation_of(c.m_axis, c.m_up);
      for (std::size_t i = 0; i + 2 < c.m_vertices.size(); i += 8)
        e.add(c.m_pos + o.x * double{c.m_vertices[i]} + o.y * double{c.m_vertices[i + 1]} +
              o.z * double{c.m_vertices[i + 2]});
    });
    each(m_extrusions, [](extent& e, const extrusion_object& x) {
      const auto o = orientation_of(x.m_axis, x.m_up);
      for (const auto& v : x.m_mesh.vertices)
        e.add(x.m_pos + o.x * double{v.position[0]} + o.y * double{v.position[1]} + o.z * double{v.position[2]});
    });
    for (const auto& [entry, trail] : m_trails)
    {
      extent e;
      for (const auto& p : trail.positions)
        e.add_ball(p, trail.radius);
      if (!e.empty)
        f(e);
    }
  }

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
    return handle<T>(handle_base<T>(this, store.size() - 1, m_entries.size() - 1, m_generation));
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
    else if constexpr (std::same_as<T, distant_light_object>)
      return m_distant_lights;
    else if constexpr (std::same_as<T, local_light_object>)
      return m_local_lights;
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
    m_distant_lights = default_lights();
    m_local_lights.clear();
    m_ambient = vec3{0.2, 0.2, 0.2};
    m_trails.clear();
    m_entries.clear();
    ++m_generation;
    m_autoscale = true; // a cleared scene starts over, like a new GlowScript canvas
    m_range = 0;
    m_autoscale_last_zx = -1;
    m_autoscale_last_zy = -1;
    m_scene_dirty = true;
  }

  // GlowScript's scene.lights = []: no lights but the ambient one
  void clear_lights() noexcept
  {
    m_distant_lights.clear();
    m_local_lights.clear();
    m_scene_dirty = true;
  }

  // The object a scene entry names, or null for a light
  object_base* object_at(std::size_t entry_idx)
  {
    const scene_entry& entry = m_entries[entry_idx];
    switch (entry.type)
    {
      case object_type::sphere:
        return &m_spheres[entry.index];
      case object_type::ellipsoid:
        return &m_ellipsoids[entry.index];
      case object_type::box:
        return &m_boxes[entry.index];
      case object_type::cylinder:
        return &m_cylinders[entry.index];
      case object_type::cone:
        return &m_cones[entry.index];
      case object_type::arrow:
        return &m_arrows[entry.index];
      case object_type::ring:
        return &m_rings[entry.index];
      case object_type::helix:
        return &m_helixes[entry.index];
      case object_type::pyramid:
        return &m_pyramids[entry.index];
      case object_type::curve:
        return &m_curves[entry.index];
      case object_type::points:
        return &m_points[entry.index];
      case object_type::label:
        return &m_labels[entry.index];
      case object_type::triangle:
        return &m_triangles[entry.index];
      case object_type::quad:
        return &m_quads[entry.index];
      case object_type::compound:
        return &m_compounds[entry.index];
      case object_type::text3d:
        return &m_text3ds[entry.index];
      case object_type::extrusion:
        return &m_extrusions[entry.index];
      case object_type::distant_light:
      case object_type::local_light:
        return nullptr;
    }
    return nullptr;
  }

  // Called once per render, as GlowScript does: an attached light moves to its object's pos plus its offset,
  // turned with the object's axis and up
  void update_lights()
  {
    for (local_light_object& light : m_local_lights)
    {
      if (!light.m_attached_to)
        continue;
      const object_base* obj = object_at(*light.m_attached_to);
      if (!obj)
        continue;
      const vec3 x = hat(obj->m_axis);
      const vec3 y = hat(obj->m_up);
      const vec3 z = cross(x, y);
      light.m_pos = obj->m_pos + x * light.m_offset.x() + y * light.m_offset.y() + z * light.m_offset.z();
    }
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

  // GlowScript's Autoscale.compute_autoscale (autoscale.js), run once per render with the canvas's size in
  // pixels. The camera keeps its direction from center; its distance is refitted only when the scene's
  // reach grows, or falls below a third of what it was at the last fit.
  // A pending scene.range is applied here too.
  void autoscale(double width, double height)
  {
    if (width <= 0 || height <= 0)
      return;
    if (m_range > 0)
    {
      place_camera(m_range, width, height);
      m_range = 0;
    }
    if (!m_autoscale)
      return;
    const vec3 ctr = m_camera.m_center;
    const double tan_hfov = std::tan(m_camera.m_fov * std::numbers::pi / 360.0);
    const double cot_hfov = 1 / tan_hfov;
    double zx = 0;
    double zy = 0;
    bool any = false;
    for_each_extent([&](const extent& e) {
      any = true;
      const double xx = std::max(std::abs(e.lo.x() - ctr.x()), std::abs(e.hi.x() - ctr.x()));
      const double yy = std::max(std::abs(e.lo.y() - ctr.y()), std::abs(e.hi.y() - ctr.y()));
      const double zz = std::max(std::abs(e.lo.z() - ctr.z()), std::abs(e.hi.z() - ctr.z()));
      zx = std::max(zx, xx * cot_hfov + zz);
      zy = std::max(zy, yy * cot_hfov + zz);
    });
    if (!any)
      return;
    const double last_zx = m_autoscale_last_zx;
    const double last_zy = m_autoscale_last_zy;
    if (!(zx > last_zx || zx < last_zx / 3 || zy > last_zy || zy < last_zy / 3))
      return;

    double range = 0;
    if (zx * height / width > zy)
      range = width >= height ? 1.1 * (height / width) * zx / cot_hfov : 1.1 * zx / cot_hfov;
    else
      range = width >= height ? 1.1 * zy / cot_hfov : 1.1 * (width / height) * zy / cot_hfov;
    m_autoscale_last_zx = zx;
    m_autoscale_last_zy = zy;
    place_camera(range, width, height);
  }

  // Moves the camera along its direction from center so the view reaches `range` from center
  void place_camera(double range, double width, double height)
  {
    const double tan_hfov = std::tan(m_camera.m_fov * std::numbers::pi / 360.0);
    const double distance = width >= height ? range / tan_hfov : range * (height / width) / tan_hfov;
    vec3 dir = m_camera.m_pos - m_camera.m_center;
    if (mag2(dir) == 0)
      dir = vec3{0, 0, 1};
    m_camera.m_pos = m_camera.m_center + hat(dir) * distance;
  }

  // Adds obj's position to the trail of scene entry `entry`, keeping the newest m_retain points
  void add_trail_point(std::size_t entry, const object_base& obj)
  {
    auto& trail = m_trails[entry];
    trail.color = obj.m_trail_color;
    trail.radius = obj.m_trail_radius;
    trail.points = obj.m_trail_type == "points";
    trail.positions.push_back(obj.m_pos);
    ++trail.added;
    trail.retain = obj.m_retain;
    if (obj.m_retain >= 0 && trail.positions.size() > static_cast<std::size_t>(obj.m_retain))
      trail.positions.erase(trail.positions.begin(), trail.positions.end() - static_cast<std::ptrdiff_t>(obj.m_retain));
  }
};

template<typename T>
T& handle_base<T>::operator*() const
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
void handle_base<T>::set_pos(const vec3& v) const
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
void handle_base<T>::set_axis(const vec3& v) const
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

template<typename T>
void handle_base<T>::rotate(double angle, std::optional<vec3> rotation_axis, std::optional<vec3> origin) const
{
  if (angle == 0)
    return;
  T& obj = **this;
  const vec3 about = rotation_axis.value_or(obj.m_axis);
  if (origin && *origin != obj.m_pos)
    set_pos(*origin + vcpp::rotate(obj.m_pos - *origin, angle, about));
  // up starts as the up the object is drawn with, which is GlowScript's: made perpendicular to axis when axis
  // was set. Axis and up then turn together, not through set_axis and set_up, which would each turn the other.
  const vec3 up = orientation_of(obj.m_axis, obj.m_up).y;
  if (diff_angle(obj.m_axis, about) > 1e-6)
    obj.m_axis = vcpp::rotate(obj.m_axis, angle, about);
  obj.m_up = vcpp::rotate(up, angle, about);
}

// GlowScript's up setter: the axis turns with up
template<typename T>
void handle_base<T>::set_up(const vec3& v) const
{
  T& obj = **this;
  const vec3 from = hat(obj.m_up);
  obj.m_up = v;
  detail::turn_with(obj.m_axis, from, v);
}

// GlowScript's length setter, including its quirk: after a zero length, a new length is set along
// (1,0,0), because it tests the old length after restoring the remembered axis
template<typename T>
void handle_base<T>::set_length(double length) const
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
void handle_base<T>::set_size(const vec3& size) const
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
// handle<T> - What sphere(...) and canvas::add return: a handle_base with GlowScript's attributes as members
//
//   auto ball = sphere(pos = vec3{0, 4, 0}, radius = 0.5);
//   ball.pos = ball.pos + v * dt;     // applies GlowScript's pos rule (trails)
//   ball.color = colors::red;
//
// Each attribute names its object as the handle does, so it stays valid as the scene grows and throws after
// clear(). Copying a handle gives another name for the same object, as in Python.
// ============================================================================

namespace detail
{
// An attribute that reads the object's Get (a member, or a function of the object) and writes it through Set:
// a handle_base setter or the object's own setter, which apply GlowScript's rule for it, or, if not given,
// by writing the member.
template<typename T, auto Get, auto Set = nullptr>
class attribute
{
public:
  using value_type = std::remove_cvref_t<std::invoke_result_t<decltype(Get), const T&>>;

  attribute() = default;
  explicit attribute(const handle_base<T>& h) : m_h(h) {}
  attribute(const attribute&) = default;

  value_type value() const { return std::invoke(Get, std::as_const(*m_h)); }
  operator value_type() const { return value(); }

  attribute& operator=(const value_type& v)
  {
    if constexpr (std::is_null_pointer_v<decltype(Set)>)
      std::invoke(Get, *m_h) = v;
    else if constexpr (std::invocable<decltype(Set), const handle_base<T>&, const value_type&>)
      std::invoke(Set, m_h, v);
    else
      std::invoke(Set, *m_h, v);
    return *this;
  }

  // Anything the value itself can be assigned: an int for a double, a string_view for a string
  template<typename V>
    requires(!std::same_as<V, value_type> && std::assignable_from<value_type&, const V&>)
  attribute& operator=(const V& v)
  {
    value_type x = value();
    x = v;
    return *this = x;
  }

  // a.pos = b.pos copies the value, not which object the attribute names
  attribute& operator=(const attribute& other) { return *this = other.value(); }

  template<typename V>
  attribute& operator+=(const V& v)
  { return *this = value_type(value() + v); }
  template<typename V>
  attribute& operator-=(const V& v)
  { return *this = value_type(value() - v); }
  attribute& operator*=(double k) { return *this = value_type(value() * k); }
  attribute& operator/=(double k) { return *this = value_type(value() / k); }

  double x() const
    requires std::same_as<value_type, vec3>
  { return value().x(); }
  double y() const
    requires std::same_as<value_type, vec3>
  { return value().y(); }
  double z() const
    requires std::same_as<value_type, vec3>
  { return value().z(); }

private:
  handle_base<T> m_h;
};

// Vector arithmetic on vector attributes. lam's operators are templates, which don't convert an attribute
// to vec3, so these do it.
template<typename A>
concept vector_attribute = requires(const A& a) {
  { a.value() } -> std::same_as<vec3>;
};

template<typename A>
concept vector_operand = vector_attribute<A> || std::same_as<A, vec3>;

template<typename A>
vec3 as_vector(const A& a)
{
  if constexpr (vector_attribute<A>)
    return a.value();
  else
    return a;
}

template<vector_operand A, vector_operand B>
  requires(vector_attribute<A> || vector_attribute<B>)
vec3 operator+(const A& a, const B& b)
{ return as_vector(a) + as_vector(b); }

template<vector_operand A, vector_operand B>
  requires(vector_attribute<A> || vector_attribute<B>)
vec3 operator-(const A& a, const B& b)
{ return as_vector(a) - as_vector(b); }

template<vector_operand A, vector_operand B>
  requires(vector_attribute<A> || vector_attribute<B>)
bool operator==(const A& a, const B& b)
{ return as_vector(a) == as_vector(b); }

template<vector_attribute A>
vec3 operator-(const A& a)
{ return -a.value(); }

template<vector_attribute A>
vec3 operator*(const A& a, double k)
{ return a.value() * k; }

template<vector_attribute A>
vec3 operator*(double k, const A& a)
{ return k * a.value(); }

template<vector_attribute A>
vec3 operator/(const A& a, double k)
{ return a.value() / k; }

// GlowScript's size, read back from the members size= fans out to
template<typename T>
vec3 size_of(const T& o)
{
  if constexpr (requires { o.m_width; })
    return vec3{o.m_length, o.m_height, o.m_width};
  else if constexpr (requires { o.m_length; })
    return vec3{o.m_length, 2 * o.m_radius, 2 * o.m_radius};
  else
    return vec3{2 * o.m_radius, 2 * o.m_radius, 2 * o.m_radius};
}

// An arrow's length is its axis's: GlowScript's arrow keeps no length of its own
inline double arrow_length(const arrow_object& o) { return mag(o.m_axis); }

inline void set_arrow_length(const handle_base<arrow_object>& h, double length)
{
  const vec3 dir = mag2(h->m_axis) > 0 ? hat(h->m_axis) : vec3{1, 0, 0};
  h.set_axis(vec3{dir.x() * length, dir.y() * length, dir.z() * length});
}

// The attributes every object has
template<typename T>
struct common_attributes
{
  attribute<T, &object_base::m_pos, &handle_base<T>::set_pos> pos;
  attribute<T, &object_base::m_axis, &handle_base<T>::set_axis> axis;
  attribute<T, &object_base::m_up, &handle_base<T>::set_up> up;
  attribute<T, &object_base::m_color> color;
  attribute<T, &object_base::m_opacity> opacity;
  attribute<T, &object_base::m_shininess> shininess;
  attribute<T, &object_base::m_emissive> emissive;
  attribute<T, &object_base::m_visible> visible;
  attribute<T, &object_base::m_texture> texture;
  attribute<T, &object_base::m_make_trail> make_trail;
  attribute<T, &object_base::m_trail_color> trail_color;
  attribute<T, &object_base::m_trail_type> trail_type;
  attribute<T, &object_base::m_trail_radius> trail_radius;
  attribute<T, &object_base::m_retain> retain;
  attribute<T, &object_base::m_interval> interval;

  common_attributes() = default;
  explicit common_attributes(const handle_base<T>& h)
    : pos(h), axis(h), up(h), color(h), opacity(h), shininess(h), emissive(h), visible(h), texture(h), make_trail(h),
      trail_color(h), trail_type(h), trail_radius(h), retain(h), interval(h)
  {}
};

// The attributes only some objects have
template<typename T>
struct own_attributes
{
  own_attributes() = default;
  explicit own_attributes(const handle_base<T>&) {}
};

template<>
struct own_attributes<sphere_object>
{
  attribute<sphere_object, &sphere_object::m_radius> radius;
  attribute<sphere_object, &size_of<sphere_object>, &handle_base<sphere_object>::set_size> size;

  own_attributes() = default;
  explicit own_attributes(const handle_base<sphere_object>& h) : radius(h), size(h) {}
};

// box, ellipsoid, pyramid: size = (length, height, width)
template<typename T>
struct lhw_attributes
{
  attribute<T, &T::m_length, &handle_base<T>::set_length> length;
  attribute<T, &T::m_height> height;
  attribute<T, &T::m_width> width;
  attribute<T, &size_of<T>, &handle_base<T>::set_size> size;

  lhw_attributes() = default;
  explicit lhw_attributes(const handle_base<T>& h) : length(h), height(h), width(h), size(h) {}
};

template<>
struct own_attributes<box_object> : lhw_attributes<box_object>
{
  using lhw_attributes::lhw_attributes;
};

template<>
struct own_attributes<ellipsoid_object> : lhw_attributes<ellipsoid_object>
{
  using lhw_attributes::lhw_attributes;
};

template<>
struct own_attributes<pyramid_object> : lhw_attributes<pyramid_object>
{
  using lhw_attributes::lhw_attributes;
};

// cylinder, cone, helix: size = (length, 2*radius, 2*radius)
template<typename T>
struct lr_attributes
{
  attribute<T, &T::m_radius> radius;
  attribute<T, &T::m_length, &handle_base<T>::set_length> length;
  attribute<T, &size_of<T>, &handle_base<T>::set_size> size;

  lr_attributes() = default;
  explicit lr_attributes(const handle_base<T>& h) : radius(h), length(h), size(h) {}
};

template<>
struct own_attributes<cylinder_object> : lr_attributes<cylinder_object>
{
  using lr_attributes::lr_attributes;
};

template<>
struct own_attributes<cone_object> : lr_attributes<cone_object>
{
  using lr_attributes::lr_attributes;
};

template<>
struct own_attributes<helix_object> : lr_attributes<helix_object>
{
  attribute<helix_object, &helix_object::m_thickness> thickness;
  attribute<helix_object, &helix_object::m_coils> coils;
  attribute<helix_object, &helix_object::m_ccw> ccw;

  own_attributes() = default;
  explicit own_attributes(const handle_base<helix_object>& h) : lr_attributes(h), thickness(h), coils(h), ccw(h) {}
};

template<>
struct own_attributes<arrow_object>
{
  attribute<arrow_object, &arrow_length, &set_arrow_length> length;
  attribute<arrow_object, &arrow_object::m_shaftwidth> shaftwidth;
  attribute<arrow_object, &arrow_object::m_headwidth> headwidth;
  attribute<arrow_object, &arrow_object::m_headlength> headlength;
  attribute<arrow_object, &arrow_object::m_round> round;

  own_attributes() = default;
  explicit own_attributes(const handle_base<arrow_object>& h)
    : length(h), shaftwidth(h), headwidth(h), headlength(h), round(h)
  {}
};

template<>
struct own_attributes<ring_object>
{
  attribute<ring_object, &ring_object::m_radius> radius;
  attribute<ring_object, &ring_object::m_thickness> thickness;

  own_attributes() = default;
  explicit own_attributes(const handle_base<ring_object>& h) : radius(h), thickness(h) {}
};

template<>
struct own_attributes<curve_object>
{
  attribute<curve_object, &curve_object::m_radius, &curve_object::set_radius> radius;
  attribute<curve_object, &curve_object::npoints> npoints; // read only

  own_attributes() = default;
  explicit own_attributes(const handle_base<curve_object>& h) : radius(h), npoints(h) {}
};

template<>
struct own_attributes<label_object>
{
  attribute<label_object, &label_object::m_text> text;
  attribute<label_object, &label_object::m_height> height;
  attribute<label_object, &label_object::m_font> font;
  attribute<label_object, &label_object::m_xoffset> xoffset;
  attribute<label_object, &label_object::m_yoffset> yoffset;
  attribute<label_object, &label_object::m_box> box;
  attribute<label_object, &label_object::m_line> line;
  attribute<label_object, &label_object::m_border> border;
  attribute<label_object, &label_object::m_background> background;

  own_attributes() = default;
  explicit own_attributes(const handle_base<label_object>& h)
    : text(h), height(h), font(h), xoffset(h), yoffset(h), box(h), line(h), border(h), background(h)
  {}
};
// Lights have none of an object's attributes, only GlowScript's own for them
template<>
struct common_attributes<distant_light_object>
{
  attribute<distant_light_object, &distant_light_object::m_direction> direction;
  attribute<distant_light_object, &distant_light_object::m_color> color;
  attribute<distant_light_object, &distant_light_object::m_visible> visible;

  common_attributes() = default;
  explicit common_attributes(const handle_base<distant_light_object>& h) : direction(h), color(h), visible(h) {}
};

template<>
struct common_attributes<local_light_object>
{
  attribute<local_light_object, &local_light_object::m_pos> pos;
  attribute<local_light_object, &local_light_object::m_color> color;
  attribute<local_light_object, &local_light_object::m_visible> visible;
  attribute<local_light_object, &local_light_object::m_offset> offset;

  common_attributes() = default;
  explicit common_attributes(const handle_base<local_light_object>& h) : pos(h), color(h), visible(h), offset(h) {}
};
} // namespace detail

template<typename T>
class handle : public handle_base<T>, public detail::common_attributes<T>, public detail::own_attributes<T>
{
public:
  handle() = default;
  handle(const handle&) = default;

  // Names the other handle's object, as Python's ball = other does. The attributes' own assignment copies
  // values, so the handle is rebuilt instead.
  handle& operator=(const handle& other)
  {
    const handle_base<T> h = other;
    std::destroy_at(this);
    ::new (static_cast<void*>(this)) handle(h);
    return *this;
  }

private:
  friend class canvas;
  explicit handle(const handle_base<T>& h)
    : handle_base<T>(h), detail::common_attributes<T>(h), detail::own_attributes<T>(h)
  {}
};

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

// ============================================================================
// VPython's object constructors: each makes the object in the selected canvas and returns its handle
//
//   auto ball = sphere(pos = vec3{0, 4, 0}, radius = 0.5);
//
// build::sphere(...) and the rest make one without adding it, for canvas::add.
// ============================================================================

template<typename... Binders>
handle<sphere_object> sphere(Binders... binders)
{ return selected().add(build::sphere(binders...)); }

template<typename... Binders>
handle<ellipsoid_object> ellipsoid(Binders... binders)
{ return selected().add(build::ellipsoid(binders...)); }

template<typename... Binders>
handle<box_object> box(Binders... binders)
{ return selected().add(build::box(binders...)); }

template<typename... Binders>
handle<cylinder_object> cylinder(Binders... binders)
{ return selected().add(build::cylinder(binders...)); }

template<typename... Binders>
handle<cone_object> cone(Binders... binders)
{ return selected().add(build::cone(binders...)); }

template<typename... Binders>
handle<arrow_object> arrow(Binders... binders)
{ return selected().add(build::arrow(binders...)); }

template<typename... Binders>
handle<ring_object> ring(Binders... binders)
{ return selected().add(build::ring(binders...)); }

template<typename... Binders>
handle<helix_object> helix(Binders... binders)
{ return selected().add(build::helix(binders...)); }

template<typename... Binders>
handle<pyramid_object> pyramid(Binders... binders)
{ return selected().add(build::pyramid(binders...)); }

template<typename... Binders>
handle<curve_object> curve(Binders... binders)
{ return selected().add(build::curve(binders...)); }

template<typename... Binders>
handle<points_object> points(Binders... binders)
{ return selected().add(build::points(binders...)); }

template<typename... Binders>
handle<label_object> label(Binders... binders)
{ return selected().add(build::label(binders...)); }

template<typename... Binders>
handle<triangle_object> triangle(Binders... binders)
{ return selected().add(build::triangle(binders...)); }

template<typename... Binders>
handle<quad_object> quad(Binders... binders)
{ return selected().add(build::quad(binders...)); }

template<typename... Binders>
handle<text3d_object> text3d(Binders... binders)
{ return selected().add(build::text3d(binders...)); }

template<typename... Binders>
handle<extrusion_object> extrusion(Binders... binders)
{ return selected().add(build::extrusion(binders...)); }

// GlowScript's lights: distant_light(direction = ..., color = ...) and local_light(pos = ..., color = ...)
template<typename... Binders>
handle<distant_light_object> distant_light(Binders... binders)
{ return selected().add(build::distant_light(binders...)); }

template<typename... Binders>
handle<local_light_object> local_light(Binders... binders)
{ return selected().add(build::local_light(binders...)); }

// GlowScript's attach_light(obj, offset = ..., color = ...): a local light that follows obj, at offset in its
// frame, in obj's colour unless given one
inline constexpr auto attach_light_params = std::tuple{param_spec<&local_light_object::m_offset, decltype(offset)>{},
                                                       param_spec<&local_light_object::m_color, decltype(color)>{}};

template<typename T, typename... Binders>
handle<local_light_object> attach_light(const handle<T>& obj, Binders... binders)
{
  local_light_object light = detail::make_light<local_light_object>(attach_light_params, binders...);
  if constexpr (!detail::names<decltype(color), Binders...>)
    light.m_color = obj->m_color;
  light.m_attached_to = obj.entry();
  auto h = selected().add(std::move(light));
  selected().update_lights();
  return h;
}

// GlowScript's compound: one object made from copies of the parts, which are hidden
template<typename... Parts>
handle<compound_object> compound(const handle<Parts>&... parts)
{
  handle<compound_object> h = selected().add(build::compound(*parts...));
  ((parts->m_visible = false), ...);
  return h;
}

} // namespace vcpp

// An attribute formats as its value: std::format("{:.2f}", ball.radius)
template<typename T, auto Get, auto Set>
struct std::formatter<vcpp::detail::attribute<T, Get, Set>, char>
  : std::formatter<typename vcpp::detail::attribute<T, Get, Set>::value_type, char>
{
  auto format(const vcpp::detail::attribute<T, Get, Set>& a, std::format_context& ctx) const
  { return std::formatter<typename vcpp::detail::attribute<T, Get, Set>::value_type, char>::format(a.value(), ctx); }
};
