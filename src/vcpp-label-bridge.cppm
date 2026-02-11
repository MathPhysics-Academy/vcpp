/*
 *  vcpp:label_bridge - JavaScript bridge for 2D label rendering
 *
 *  Provides:
 *  - Project 3D positions to 2D screen coordinates
 *  - Call JavaScript to render text using Canvas 2D overlay
 *  - Support for labels with backgrounds, borders, and lines
 */

module;

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

import std;

export module vcpp:label_bridge;

import :vec;
import :render_types;

export namespace vcpp::label_bridge
{

#ifdef __EMSCRIPTEN__

// ============================================================================
// JavaScript functions for label rendering
// ============================================================================

EM_JS(void, js_init_label_canvas, (), {
  // Create overlay canvas for labels if it doesn't exist
  if (!window.vcppLabelCanvas)
  {
    var canvas = document.getElementById('canvas');
    var overlay = document.createElement('canvas');
    overlay.id = 'vcpp-label-overlay';
    overlay.style.position = 'absolute';
    overlay.style.left = canvas.offsetLeft + 'px';
    overlay.style.top = canvas.offsetTop + 'px';
    overlay.style.pointerEvents = 'none';
    overlay.width = canvas.width;
    overlay.height = canvas.height;
    canvas.parentElement.style.position = 'relative';
    canvas.parentElement.appendChild(overlay);
    window.vcppLabelCanvas = overlay;
    window.vcppLabelCtx = overlay.getContext('2d');
  }
});

EM_JS(void, js_clear_labels, (), {
  if (window.vcppLabelCtx)
  {
    var canvas = window.vcppLabelCanvas;
    var mainCanvas = document.getElementById('canvas');
    // Sync size with main canvas
    if (canvas.width != mainCanvas.width || canvas.height != mainCanvas.height)
    {
      canvas.width = mainCanvas.width;
      canvas.height = mainCanvas.height;
      canvas.style.left = mainCanvas.offsetLeft + 'px';
      canvas.style.top = mainCanvas.offsetTop + 'px';
    }
    window.vcppLabelCtx.clearRect(0, 0, canvas.width, canvas.height);
  }
});

EM_JS(void, js_draw_label,
      (double screen_x, double screen_y, double pos_x, double pos_y, const char* text, double height,
       const char* font, double r, double g, double b, double opacity, int show_box, double bg_r, double bg_g,
       double bg_b, double bg_opacity, double border, int show_line),
      {
        if (!window.vcppLabelCtx)
          return;

        var ctx = window.vcppLabelCtx;
        var label_text = UTF8ToString(text);
        var font_name = UTF8ToString(font);

        ctx.save();

        // Set font
        ctx.font = height + 'px ' + font_name;
        ctx.textBaseline = 'middle';
        ctx.textAlign = 'center';

        var metrics = ctx.measureText(label_text);
        var text_width = metrics.width;
        var text_height = height;

        var box_x = screen_x - text_width / 2 - border;
        var box_y = screen_y - text_height / 2 - border;
        var box_w = text_width + border * 2;
        var box_h = text_height + border * 2;

        // Draw line from 3D pos to label
        if (show_line)
        {
          ctx.strokeStyle = 'rgba(' + Math.floor(r * 255) + ',' + Math.floor(g * 255) + ',' + Math.floor(b * 255) +
                            ',' + opacity + ')';
          ctx.lineWidth = 1;
          ctx.beginPath();
          ctx.moveTo(pos_x, pos_y);
          ctx.lineTo(screen_x, screen_y);
          ctx.stroke();
        }

        // Draw background box
        if (show_box)
        {
          ctx.fillStyle = 'rgba(' + Math.floor(bg_r * 255) + ',' + Math.floor(bg_g * 255) + ',' +
                          Math.floor(bg_b * 255) + ',' + bg_opacity + ')';
          ctx.fillRect(box_x, box_y, box_w, box_h);

          ctx.strokeStyle = 'rgba(' + Math.floor(r * 255) + ',' + Math.floor(g * 255) + ',' + Math.floor(b * 255) +
                            ',' + opacity + ')';
          ctx.lineWidth = 1;
          ctx.strokeRect(box_x, box_y, box_w, box_h);
        }

        // Draw text
        ctx.fillStyle =
          'rgba(' + Math.floor(r * 255) + ',' + Math.floor(g * 255) + ',' + Math.floor(b * 255) + ',' + opacity + ')';
        ctx.fillText(label_text, screen_x, screen_y);

        ctx.restore();
      });

// ============================================================================
// Project 3D position to 2D screen coordinates
// ============================================================================

inline bool project_to_screen(const vec3& world_pos, const render::camera_uniforms& cam, int screen_width,
                              int screen_height, double& out_x, double& out_y)
{
  // Apply view-projection matrix
  float px = static_cast<float>(world_pos.x());
  float py = static_cast<float>(world_pos.y());
  float pz = static_cast<float>(world_pos.z());

  // Multiply by view_projection matrix (column-major)
  const auto& m = cam.view_projection.data;
  float clip_x = m[0] * px + m[4] * py + m[8] * pz + m[12];
  float clip_y = m[1] * px + m[5] * py + m[9] * pz + m[13];
  float clip_z = m[2] * px + m[6] * py + m[10] * pz + m[14];
  float clip_w = m[3] * px + m[7] * py + m[11] * pz + m[15];

  // Check if behind camera
  if (clip_w <= 0.001f)
    return false;

  // Perspective divide
  float ndc_x = clip_x / clip_w;
  float ndc_y = clip_y / clip_w;

  // NDC to screen coordinates
  out_x = (ndc_x + 1.0) * 0.5 * screen_width;
  out_y = (1.0 - ndc_y) * 0.5 * screen_height; // Flip Y for screen space

  return true;
}

// ============================================================================
// Initialize label rendering
// ============================================================================

inline void init() { js_init_label_canvas(); }

// ============================================================================
// Clear all labels (call at start of frame)
// ============================================================================

inline void clear() { js_clear_labels(); }

// ============================================================================
// Draw a single label
// ============================================================================

inline void draw_label(const vec3& world_pos, double xoffset, double yoffset, const std::string& text, double height,
                       const std::string& font, const vec3& color, double opacity, bool show_box, const vec3& bg_color,
                       double bg_opacity, double border, bool show_line, const render::camera_uniforms& cam,
                       int screen_width, int screen_height)
{
  double pos_x, pos_y;
  if (!project_to_screen(world_pos, cam, screen_width, screen_height, pos_x, pos_y))
    return; // Behind camera

  double label_x = pos_x + xoffset;
  double label_y = pos_y - yoffset; // Screen Y is inverted

  js_draw_label(label_x, label_y, pos_x, pos_y, text.c_str(), height, font.c_str(), color.x(), color.y(), color.z(),
                opacity, show_box ? 1 : 0, bg_color.x(), bg_color.y(), bg_color.z(), bg_opacity, border,
                show_line ? 1 : 0);
}

#else

// Stub implementations for non-Emscripten builds
inline void init() {}
inline void clear() {}
inline void draw_label(const vec3&, double, double, const std::string&, double, const std::string&, const vec3&, double,
                       bool, const vec3&, double, double, bool, const render::camera_uniforms&, int, int)
{
}

#endif

} // namespace vcpp::label_bridge
