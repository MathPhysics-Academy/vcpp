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
  if (m.left_down && !g_input.ctrl_held && !g_input.shift_held)
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
       cam.pan(r * (-dx * scale) + u * (dy * scale));
       c.mark_dirty();
    }
  }

  // Scroll wheel: zoom
  // Trackpads generate scroll events.
  if (m.scroll_delta != 0)
  {
    // Reduce sensitivity for trackpads (which generate high delta)
    double sensitivity = g_input_config.zoom_sensitivity;
    // Cap delta per frame to avoid jumping?
    // Or just use factor.
    double factor = 1.0 - m.scroll_delta * sensitivity;
    if (factor > 0.01 && factor < 100.0) 
    {
      cam.zoom(factor);
      c.mark_dirty();
    }
    m.scroll_delta = 0; // consume scroll
  }

  // Update last position for next frame
  m.last_x = m.x;
  m.last_y = m.y;
}

} // namespace vcpp
