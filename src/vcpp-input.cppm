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

  // Left-drag: orbit
  if (m.left_down && !g_input.ctrl_held && !g_input.shift_held)
  {
    if (dx != 0 || dy != 0)
    {
      cam.orbit(-dx * g_input_config.orbit_sensitivity, -dy * g_input_config.orbit_sensitivity);
      c.mark_dirty();
    }
  }

  // Right-drag or Ctrl+Left-drag: pan
  if (m.right_down || (m.left_down && g_input.ctrl_held))
  {
    if (dx != 0 || dy != 0)
    {
      vec3 r = cam.right();
      vec3 u = cam.m_up;
      double scale = g_input_config.pan_sensitivity;
      cam.pan(r * (-dx * scale) + u * (dy * scale));
      c.mark_dirty();
    }
  }

  // Middle-drag: zoom via drag
  if (m.middle_down)
  {
    if (dy != 0)
    {
      double factor = 1.0 + dy * g_input_config.zoom_drag_sensitivity;
      cam.zoom(factor);
      c.mark_dirty();
    }
  }

  // Scroll wheel: zoom
  if (m.scroll_delta != 0)
  {
    double factor = 1.0 - m.scroll_delta * g_input_config.zoom_sensitivity;
    if (factor > 0.01) // prevent negative/zero zoom
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
