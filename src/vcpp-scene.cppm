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
    std::vector<double> timestamps;
    vec3 color{1, 1, 1};
    double radius{0.02};
    mutable bool dirty{true};
  };
  std::unordered_map<std::size_t, trail_data> m_trails; // keyed by scene entry index

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

  std::size_t add(curve_object obj)
  {
    std::size_t idx = m_curves.size();
    m_curves.push_back(std::move(obj));
    m_entries.push_back({object_type::curve, idx, true});
    m_scene_dirty = true;
    return m_entries.size() - 1;
  }

  std::size_t add(points_object obj)
  {
    std::size_t idx = m_points.size();
    m_points.push_back(std::move(obj));
    m_entries.push_back({object_type::points, idx, true});
    m_scene_dirty = true;
    return m_entries.size() - 1;
  }

  std::size_t add(label_object obj)
  {
    std::size_t idx = m_labels.size();
    m_labels.push_back(std::move(obj));
    m_entries.push_back({object_type::label, idx, true});
    m_scene_dirty = true;
    return m_entries.size() - 1;
  }

  std::size_t add(triangle_object obj)
  {
    std::size_t idx = m_triangles.size();
    m_triangles.push_back(std::move(obj));
    m_entries.push_back({object_type::triangle, idx, true});
    m_scene_dirty = true;
    return m_entries.size() - 1;
  }

  std::size_t add(quad_object obj)
  {
    std::size_t idx = m_quads.size();
    m_quads.push_back(std::move(obj));
    m_entries.push_back({object_type::quad, idx, true});
    m_scene_dirty = true;
    return m_entries.size() - 1;
  }

  std::size_t add(compound_object obj)
  {
    std::size_t idx = m_compounds.size();
    m_compounds.push_back(std::move(obj));
    m_entries.push_back({object_type::compound, idx, true});
    m_scene_dirty = true;
    return m_entries.size() - 1;
  }

  std::size_t add(text3d_object obj)
  {
    std::size_t idx = m_text3ds.size();
    m_text3ds.push_back(std::move(obj));
    m_entries.push_back({object_type::text3d, idx, true});
    m_scene_dirty = true;
    return m_entries.size() - 1;
  }

  std::size_t add(extrusion_object obj)
  {
    std::size_t idx = m_extrusions.size();
    m_extrusions.push_back(std::move(obj));
    m_entries.push_back({object_type::extrusion, idx, true});
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
    m_scene_dirty = true;
  }

  // ========== Trail Management ==========

  void update_trails(double current_time)
  {
    // Iterate through all scene entries and update trails for objects with make_trail=true
    for (std::size_t entry_idx = 0; entry_idx < m_entries.size(); ++entry_idx)
    {
      const auto& entry = m_entries[entry_idx];
      const object_base* obj = nullptr;
      vec3 trail_col{1, 1, 1};
      double retain = -1.0;

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

      if (!obj || !obj->m_make_trail)
        continue;

      trail_col = obj->m_trail_color;
      retain = obj->m_retain;

      // Get or create trail data
      auto& trail = m_trails[entry_idx];
      trail.color = trail_col;

      // Add current position
      trail.positions.push_back(obj->m_pos);
      trail.timestamps.push_back(current_time);
      trail.dirty = true;

      // Prune old points if retain is positive
      if (retain > 0)
      {
        while (!trail.timestamps.empty() && (current_time - trail.timestamps.front()) > retain)
        {
          trail.positions.erase(trail.positions.begin());
          trail.timestamps.erase(trail.timestamps.begin());
          trail.dirty = true;
        }
      }
    }
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

inline void select(canvas& c) noexcept { current_canvas = &c; }

inline canvas& selected() noexcept { return *current_canvas; }

} // namespace vcpp
