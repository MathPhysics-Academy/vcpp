/*
 *  vcpp:graph_bridge - Emscripten bridge for 2D graphing
 *
 *  Provides communication between C++ graph objects and JavaScript
 *  Canvas 2D rendering. Uses command queue pattern for performance:
 *  1. plot() calls queue data updates
 *  2. flush_updates() sends batched data to JS once per frame
 */

module;

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <emscripten/val.h>
#endif

import std;

export module vcpp:graph_bridge;

import :vec;
import :graph_base;
import :graph_objects;

#ifdef __EMSCRIPTEN__

// ============================================================================
// JavaScript interface functions (EM_JS)
// ============================================================================

EM_JS(void, js_create_graph, (
    int id, int width, int height,
    float bg_r, float bg_g, float bg_b,
    float fg_r, float fg_g, float fg_b,
    const char* title, const char* xtitle, const char* ytitle
), {
    if (typeof window.vcppGraphManager !== 'undefined') {
        window.vcppGraphManager.createGraph(id, width, height,
            [bg_r, bg_g, bg_b], [fg_r, fg_g, fg_b],
            UTF8ToString(title), UTF8ToString(xtitle), UTF8ToString(ytitle));
    }
});

EM_JS(void, js_create_plot, (
    int graph_id, int plot_id, int type,
    float r, float g, float b,
    const char* label,
    float width_or_radius, float delta
), {
    if (typeof window.vcppGraphManager !== 'undefined') {
        window.vcppGraphManager.createPlot(graph_id, plot_id, type,
            [r, g, b], UTF8ToString(label), width_or_radius, delta);
    }
});

EM_JS(void, js_add_point, (int graph_id, int plot_id, double x, double y), {
    if (typeof window.vcppGraphManager !== 'undefined') {
        window.vcppGraphManager.addPoint(graph_id, plot_id, x, y);
    }
});

EM_JS(void, js_add_points_batch, (int graph_id, int plot_id, const double* data, int count), {
    if (typeof window.vcppGraphManager !== 'undefined') {
        var points = [];
        for (var i = 0; i < count; i++) {
            points.push({x: HEAPF64[(data >> 3) + i * 2], y: HEAPF64[(data >> 3) + i * 2 + 1]});
        }
        window.vcppGraphManager.addPointsBatch(graph_id, plot_id, points);
    }
});

EM_JS(void, js_update_graph_bounds, (
    int graph_id,
    double xmin, double xmax, double ymin, double ymax
), {
    if (typeof window.vcppGraphManager !== 'undefined') {
        window.vcppGraphManager.updateBounds(graph_id, xmin, xmax, ymin, ymax);
    }
});

EM_JS(void, js_redraw_graph, (int graph_id), {
    if (typeof window.vcppGraphManager !== 'undefined') {
        window.vcppGraphManager.redraw(graph_id);
    }
});

EM_JS(void, js_clear_plot, (int graph_id, int plot_id), {
    if (typeof window.vcppGraphManager !== 'undefined') {
        window.vcppGraphManager.clearPlot(graph_id, plot_id);
    }
});

EM_JS(void, js_remove_graph, (int graph_id), {
    if (typeof window.vcppGraphManager !== 'undefined') {
        window.vcppGraphManager.removeGraph(graph_id);
    }
});

EM_JS(void, js_set_plot_visible, (int graph_id, int plot_id, bool visible), {
    if (typeof window.vcppGraphManager !== 'undefined') {
        window.vcppGraphManager.setPlotVisible(graph_id, plot_id, visible);
    }
});

#endif // __EMSCRIPTEN__

export namespace vcpp::graph_bridge
{

// ============================================================================
// Command queue for batched updates
// ============================================================================

struct point_command
{
  std::size_t graph_id;
  std::size_t plot_id;
  double x;
  double y;
};

// Command queue
inline std::vector<point_command> g_point_queue;

// ============================================================================
// sync_graph - Create graph on JS side
// ============================================================================

inline void sync_graph(graph_object& g)
{
#ifdef __EMSCRIPTEN__
  js_create_graph(
    static_cast<int>(g.m_id),
    g.m_width, g.m_height,
    static_cast<float>(g.m_background.x()),
    static_cast<float>(g.m_background.y()),
    static_cast<float>(g.m_background.z()),
    static_cast<float>(g.m_foreground.x()),
    static_cast<float>(g.m_foreground.y()),
    static_cast<float>(g.m_foreground.z()),
    g.m_title.c_str(),
    g.m_xtitle.c_str(),
    g.m_ytitle.c_str()
  );
  g.m_dirty = false;
#else
  (void)g;
#endif
}

// ============================================================================
// sync_gcurve - Create gcurve plot on JS side
// ============================================================================

inline void sync_gcurve(gcurve_object& curve)
{
#ifdef __EMSCRIPTEN__
  js_create_plot(
    static_cast<int>(curve.m_graph_id),
    static_cast<int>(curve.m_plot_id),
    0, // type: gcurve
    static_cast<float>(curve.m_color.x()),
    static_cast<float>(curve.m_color.y()),
    static_cast<float>(curve.m_color.z()),
    curve.m_label.c_str(),
    static_cast<float>(curve.m_width),
    0.0f // delta not used for gcurve
  );
  curve.m_dirty = false;
#else
  (void)curve;
#endif
}

// ============================================================================
// sync_gdots - Create gdots plot on JS side
// ============================================================================

inline void sync_gdots(gdots_object& dots)
{
#ifdef __EMSCRIPTEN__
  js_create_plot(
    static_cast<int>(dots.m_graph_id),
    static_cast<int>(dots.m_plot_id),
    1, // type: gdots
    static_cast<float>(dots.m_color.x()),
    static_cast<float>(dots.m_color.y()),
    static_cast<float>(dots.m_color.z()),
    dots.m_label.c_str(),
    static_cast<float>(dots.m_radius),
    0.0f // delta not used for gdots
  );
  dots.m_dirty = false;
#else
  (void)dots;
#endif
}

// ============================================================================
// sync_gvbars - Create gvbars plot on JS side
// ============================================================================

inline void sync_gvbars(gvbars_object& bars)
{
#ifdef __EMSCRIPTEN__
  js_create_plot(
    static_cast<int>(bars.m_graph_id),
    static_cast<int>(bars.m_plot_id),
    2, // type: gvbars
    static_cast<float>(bars.m_color.x()),
    static_cast<float>(bars.m_color.y()),
    static_cast<float>(bars.m_color.z()),
    bars.m_label.c_str(),
    0.0f, // width not used for gvbars
    static_cast<float>(bars.m_delta)
  );
  bars.m_dirty = false;
#else
  (void)bars;
#endif
}

// ============================================================================
// queue_point - Queue a data point for batch sending
// ============================================================================

inline void queue_point(std::size_t graph_id, std::size_t plot_id, double x, double y)
{
  g_point_queue.push_back({graph_id, plot_id, x, y});
}

// ============================================================================
// flush_updates - Send all queued updates to JS
//
// Called once per frame from the render loop.
// ============================================================================

inline void flush_updates()
{
#ifdef __EMSCRIPTEN__
  // Group points by graph/plot for efficient batching
  std::unordered_map<std::size_t, std::vector<point_command>> grouped;

  for (const auto& cmd : g_point_queue)
  {
    std::size_t key = (cmd.graph_id << 16) | cmd.plot_id;
    grouped[key].push_back(cmd);
  }

  // Send batched updates
  for (const auto& [key, commands] : grouped)
  {
    if (commands.empty()) continue;

    std::size_t graph_id = commands[0].graph_id;
    std::size_t plot_id = commands[0].plot_id;

    // Prepare data array for batch send
    std::vector<double> data;
    data.reserve(commands.size() * 2);
    for (const auto& cmd : commands)
    {
      data.push_back(cmd.x);
      data.push_back(cmd.y);
    }

    js_add_points_batch(
      static_cast<int>(graph_id),
      static_cast<int>(plot_id),
      data.data(),
      static_cast<int>(commands.size())
    );
  }

  // Update auto-scale bounds for all dirty graphs
  for (auto& g : g_graphs.m_graphs)
  {
    if (g.m_dirty)
    {
      // Compute auto-scale bounds from all plots
      double xmin = std::numeric_limits<double>::max();
      double xmax = std::numeric_limits<double>::lowest();
      double ymin = std::numeric_limits<double>::max();
      double ymax = std::numeric_limits<double>::lowest();
      bool has_data = false;

      // Check gcurves
      for (auto& curve : g_graphs.m_gcurves)
      {
        if (curve.m_graph_id == g.m_id && !curve.m_data.empty())
        {
          for (const auto& [x, y] : curve.m_data)
          {
            xmin = std::min(xmin, x);
            xmax = std::max(xmax, x);
            ymin = std::min(ymin, y);
            ymax = std::max(ymax, y);
          }
          has_data = true;
        }
      }

      // Check gdots
      for (auto& dots : g_graphs.m_gdots)
      {
        if (dots.m_graph_id == g.m_id && !dots.m_data.empty())
        {
          for (const auto& [x, y] : dots.m_data)
          {
            xmin = std::min(xmin, x);
            xmax = std::max(xmax, x);
            ymin = std::min(ymin, y);
            ymax = std::max(ymax, y);
          }
          has_data = true;
        }
      }

      // Check gvbars
      for (auto& bars : g_graphs.m_gvbars)
      {
        if (bars.m_graph_id == g.m_id && !bars.m_data.empty())
        {
          for (const auto& [x, y] : bars.m_data)
          {
            xmin = std::min(xmin, x);
            xmax = std::max(xmax, x);
            ymin = std::min(ymin, 0.0); // Bars start at 0
            ymax = std::max(ymax, y);
          }
          has_data = true;
        }
      }

      if (has_data)
      {
        // Add padding
        double x_range = xmax - xmin;
        double y_range = ymax - ymin;
        if (x_range < 1e-10) x_range = 1.0;
        if (y_range < 1e-10) y_range = 1.0;

        g.m_auto_xmin = xmin - x_range * 0.05;
        g.m_auto_xmax = xmax + x_range * 0.05;
        g.m_auto_ymin = ymin - y_range * 0.05;
        g.m_auto_ymax = ymax + y_range * 0.05;

        js_update_graph_bounds(
          static_cast<int>(g.m_id),
          g.effective_xmin(), g.effective_xmax(),
          g.effective_ymin(), g.effective_ymax()
        );
      }

      js_redraw_graph(static_cast<int>(g.m_id));
      g.m_dirty = false;
    }
  }

  // Clear the queue
  g_point_queue.clear();
#endif
}

// ============================================================================
// sync_all - Sync all graphs and plots to JS
//
// Call this after creating graphs/plots to initialize JS side.
// ============================================================================

inline void sync_all()
{
#ifdef __EMSCRIPTEN__
  // Sync all graphs first
  for (auto& g : g_graphs.m_graphs)
  {
    sync_graph(g);
  }

  // Sync all gcurves
  for (auto& curve : g_graphs.m_gcurves)
  {
    sync_gcurve(curve);
  }

  // Sync all gdots
  for (auto& dots : g_graphs.m_gdots)
  {
    sync_gdots(dots);
  }

  // Sync all gvbars
  for (auto& bars : g_graphs.m_gvbars)
  {
    sync_gvbars(bars);
  }
#endif
}

// ============================================================================
// clear_all - Remove all graphs from JS
// ============================================================================

inline void clear_all()
{
#ifdef __EMSCRIPTEN__
  for (auto& g : g_graphs.m_graphs)
  {
    js_remove_graph(static_cast<int>(g.m_id));
  }
#endif
  g_graphs.clear();
  g_point_queue.clear();
}

} // namespace vcpp::graph_bridge
