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

export namespace vcpp {

// ============================================================================
// Camera
// ============================================================================

struct camera
{
  vec3   m_pos{0, 0, 10};     // camera position
  vec3   m_center{0, 0, 0};   // look-at point
  vec3   m_up{0, 1, 0};       // up direction
  double m_fov{60.0};         // field of view (degrees)
  double m_near{0.1};         // near clipping plane
  double m_far{1000.0};       // far clipping plane

  // Computed: forward direction
  constexpr vec3 forward() const noexcept
  {
    return hat(m_center - m_pos);
  }

  // Computed: right direction
  constexpr vec3 right() const noexcept
  {
    return hat(cross(forward(), m_up));
  }

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
    if (new_dist < 0.1) new_dist = 0.1;  // minimum distance
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
  vec3   m_pos{0, 10, 10};
  vec3   m_color{1, 1, 1};
  double m_intensity{1.0};
  bool   m_directional{false};  // true = directional, false = point light

  // For directional lights, m_pos is treated as direction
  constexpr vec3 direction() const noexcept
  {
    return m_directional ? hat(m_pos) : vec3{0, 0, 0};
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
  pyramid
};

// ============================================================================
// Scene Entry (reference to an object in the scene)
// ============================================================================

struct scene_entry
{
  object_type type;
  std::size_t index;    // index into type-specific storage
  bool        dirty{true};
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
  int    m_width{800};
  int    m_height{600};
  vec3   m_background{0, 0, 0};
  bool   m_visible{true};
  bool   m_resizable{true};
  std::string m_title{"VCpp"};
  std::string m_caption{};

  // ========== Camera ==========
  camera m_camera{};

  // ========== Lighting ==========
  std::vector<light> m_lights{
    light{{0, 10, 10}, {1, 1, 1}, 1.0, false}  // default light
  };
  vec3 m_ambient{0.2, 0.2, 0.2};

  // ========== Object Storage (type-specific for cache efficiency) ==========
  std::vector<sphere_object> m_spheres;
  std::vector<ellipsoid_object> m_ellipsoids;
  std::vector<box_object>      m_boxes;
  std::vector<cylinder_object> m_cylinders;
  std::vector<cone_object>     m_cones;
  std::vector<arrow_object>    m_arrows;
  std::vector<ring_object>     m_rings;
  std::vector<helix_object>    m_helixes;
  std::vector<pyramid_object>  m_pyramids;

  // ========== Scene Graph ==========
  std::vector<scene_entry> m_entries;

  // ========== Dirty Tracking ==========
  bool m_scene_dirty{true};

  // ========== Object Registration ==========

  std::size_t add(sphere_object obj)
  {
    std::size_t idx = m_spheres.size();
    m_spheres.push_back(std::move(obj));
    m_entries.push_back({object_type::sphere, idx, true});
    m_scene_dirty = true;
    return m_entries.size() - 1;
  }

  std::size_t add(ellipsoid_object obj)
  {
    std::size_t idx = m_ellipsoids.size();
    m_ellipsoids.push_back(std::move(obj));
    m_entries.push_back({object_type::ellipsoid, idx, true});
    m_scene_dirty = true;
    return m_entries.size() - 1;
  }

  std::size_t add(box_object obj)
  {
    std::size_t idx = m_boxes.size();
    m_boxes.push_back(std::move(obj));
    m_entries.push_back({object_type::box, idx, true});
    m_scene_dirty = true;
    return m_entries.size() - 1;
  }

  std::size_t add(cylinder_object obj)
  {
    std::size_t idx = m_cylinders.size();
    m_cylinders.push_back(std::move(obj));
    m_entries.push_back({object_type::cylinder, idx, true});
    m_scene_dirty = true;
    return m_entries.size() - 1;
  }

  std::size_t add(cone_object obj)
  {
    std::size_t idx = m_cones.size();
    m_cones.push_back(std::move(obj));
    m_entries.push_back({object_type::cone, idx, true});
    m_scene_dirty = true;
    return m_entries.size() - 1;
  }

  std::size_t add(arrow_object obj)
  {
    std::size_t idx = m_arrows.size();
    m_arrows.push_back(std::move(obj));
    m_entries.push_back({object_type::arrow, idx, true});
    m_scene_dirty = true;
    return m_entries.size() - 1;
  }

  std::size_t add(ring_object obj)
  {
    std::size_t idx = m_rings.size();
    m_rings.push_back(std::move(obj));
    m_entries.push_back({object_type::ring, idx, true});
    m_scene_dirty = true;
    return m_entries.size() - 1;
  }

  std::size_t add(helix_object obj)
  {
    std::size_t idx = m_helixes.size();
    m_helixes.push_back(std::move(obj));
    m_entries.push_back({object_type::helix, idx, true});
    m_scene_dirty = true;
    return m_entries.size() - 1;
  }

  std::size_t add(pyramid_object obj)
  {
    std::size_t idx = m_pyramids.size();
    m_pyramids.push_back(std::move(obj));
    m_entries.push_back({object_type::pyramid, idx, true});
    m_scene_dirty = true;
    return m_entries.size() - 1;
  }

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
    m_boxes.clear();
    m_cylinders.clear();
    m_cones.clear();
    m_arrows.clear();
    m_rings.clear();
    m_helixes.clear();
    m_pyramids.clear();
    m_entries.clear();
    m_scene_dirty = true;
  }
};

// ============================================================================
// Global Default Scene (like VPython's 'scene')
// ============================================================================

inline canvas scene{};

// ============================================================================
// Canvas Selection (for multi-canvas support)
// ============================================================================

inline canvas* current_canvas = &scene;

inline void select(canvas& c) noexcept
{
  current_canvas = &c;
}

inline canvas& selected() noexcept
{
  return *current_canvas;
}

} // namespace vcpp
