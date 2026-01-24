/*
 *  vcpp:graph_objects - Concrete graph and plot types
 *
 *  Provides:
 *  - graph_object: Container for plots (VPython's graph())
 *  - gcurve_object: Connected line plots
 *  - gdots_object: Scatter plots
 *  - gvbars_object: Vertical bar charts
 *
 *  Factory functions follow VPython-style named parameters:
 *    auto g = graph(title="Energy", xtitle="t (s)", width=500);
 *    auto curve = gcurve(graph_ref=g, color=colors::red, label="KE");
 *    curve.plot(t, energy);
 */

module;

import std;

export module vcpp:graph_objects;

import lam.symbols;
import :vec;
import :color;
import :props;
import :traits;
import :graph_base;

export namespace vcpp
{

using namespace lam::symbols;

// ============================================================================
// Global ID counter for graphs and plots
// ============================================================================

namespace detail
{
inline std::size_t next_graph_id = 1;
inline std::size_t next_plot_id = 1;
} // namespace detail

// ============================================================================
// graph_object - 2D plot container
//
// Not a scene object - manages 2D Canvas rendering via JS bridge.
// ============================================================================

struct graph_object : graph_base
{
  // Storage for plots within this graph
  std::vector<std::size_t> m_plot_ids;

  // Register a plot with this graph
  void register_plot(std::size_t plot_id)
  {
    m_plot_ids.push_back(plot_id);
    m_dirty = true;
  }
};

// Parameter specs for graph_object (numeric types only - strings handled separately)
template<>
struct object_params<graph_object>
{
  static constexpr auto value = std::tuple{
    param_spec<&graph_base::m_width, decltype(width), 640>{},
    param_spec<&graph_base::m_height, decltype(height), 400>{},
    param_spec<&graph_base::m_background, decltype(background), vec3{0.1, 0.1, 0.15}>{},
    param_spec<&graph_base::m_foreground, decltype(foreground), vec3{0.9, 0.9, 0.9}>{},
    param_spec<&graph_base::m_xmin, decltype(xmin), 0.0>{},
    param_spec<&graph_base::m_xmax, decltype(xmax), 0.0>{},
    param_spec<&graph_base::m_ymin, decltype(ymin), 0.0>{},
    param_spec<&graph_base::m_ymax, decltype(ymax), 0.0>{},
    param_spec<&graph_base::m_fast, decltype(fast), true>{},
    param_spec<&graph_base::m_visible, decltype(visible), true>{}
  };
};

// Factory function for graph
template<typename... Binders>
graph_object graph(Binders... binders)
{
  auto params = substitution(binders...);
  graph_object obj{};
  obj.m_id = detail::next_graph_id++;

  // Apply numeric parameters
  apply_params(obj, params, object_params<graph_object>::value);

  // Handle string parameters manually (std::string can't be NTTP)
  if constexpr (is_bound<decltype(title), decltype(params)>)
    obj.m_title = title(params);
  if constexpr (is_bound<decltype(xtitle), decltype(params)>)
    obj.m_xtitle = xtitle(params);
  if constexpr (is_bound<decltype(ytitle), decltype(params)>)
    obj.m_ytitle = ytitle(params);

  return obj;
}

// ============================================================================
// gcurve_object - Connected line plot
//
// Draws connected points as a line with optional markers.
// ============================================================================

struct gcurve_object : plot_base
{
  double m_width{1.0};         // Line width
  bool m_markers{false};       // Show markers at points
  double m_marker_radius{3.0}; // Marker size in pixels
  bool m_dot{false};           // Show dot at last point
  double m_dot_radius{4.0};    // Dot size
  vec3 m_dot_color{1, 0, 0};   // Dot color (red default)

  // Convenience: plot single point
  void plot(double x, double y)
  {
    plot_base::plot(x, y);
  }

  // Convenience: plot from separate vectors
  void plot(const std::vector<double>& x, const std::vector<double>& y)
  {
    std::size_t n = std::min(x.size(), y.size());
    for (std::size_t i = 0; i < n; ++i)
    {
      plot_base::plot(x[i], y[i]);
    }
  }
};

// Parameter specs for gcurve_object (numeric types only - strings handled separately)
template<>
struct object_params<gcurve_object>
{
  static constexpr auto value = std::tuple{
    param_spec<&plot_base::m_color, decltype(color), vec3{0, 0, 0}>{},
    param_spec<&plot_base::m_visible, decltype(visible), true>{},
    param_spec<&plot_base::m_legend, decltype(legend), true>{},
    param_spec<&gcurve_object::m_width, decltype(width), 1.0>{},
    param_spec<&gcurve_object::m_markers, decltype(markers), false>{},
    param_spec<&gcurve_object::m_marker_radius, decltype(marker_radius), 3.0>{},
    param_spec<&gcurve_object::m_dot, decltype(show_dot), false>{},
    param_spec<&gcurve_object::m_dot_radius, decltype(dot_radius), 4.0>{},
    param_spec<&gcurve_object::m_dot_color, decltype(dot_color), vec3{1, 0, 0}>{}
  };
};

// Factory function for gcurve
template<typename... Binders>
gcurve_object gcurve(Binders... binders)
{
  auto params = substitution(binders...);
  gcurve_object obj{};
  obj.m_plot_id = detail::next_plot_id++;

  // Apply numeric parameters
  apply_params(obj, params, object_params<gcurve_object>::value);

  // Handle string parameters manually
  if constexpr (is_bound<decltype(label), decltype(params)>)
    obj.m_label = label(params);

  // Extract graph reference if provided
  if constexpr (is_bound<decltype(graph_ref), decltype(params)>)
  {
    auto& g = graph_ref(params);
    obj.m_graph_id = g.m_id;
  }

  return obj;
}

// ============================================================================
// gdots_object - Scatter plot
//
// Draws individual points without connecting lines.
// ============================================================================

struct gdots_object : plot_base
{
  double m_radius{3.0}; // Dot radius in pixels

  // Convenience: plot single point
  void plot(double x, double y)
  {
    plot_base::plot(x, y);
  }
};

// Parameter specs for gdots_object (numeric types only)
template<>
struct object_params<gdots_object>
{
  static constexpr auto value = std::tuple{
    param_spec<&plot_base::m_color, decltype(color), vec3{0, 0, 0}>{},
    param_spec<&plot_base::m_visible, decltype(visible), true>{},
    param_spec<&plot_base::m_legend, decltype(legend), true>{},
    param_spec<&gdots_object::m_radius, decltype(radius), 3.0>{}
  };
};

// Factory function for gdots
template<typename... Binders>
gdots_object gdots(Binders... binders)
{
  auto params = substitution(binders...);
  gdots_object obj{};
  obj.m_plot_id = detail::next_plot_id++;

  // Apply numeric parameters
  apply_params(obj, params, object_params<gdots_object>::value);

  // Handle string parameters manually
  if constexpr (is_bound<decltype(label), decltype(params)>)
    obj.m_label = label(params);

  // Extract graph reference if provided
  if constexpr (is_bound<decltype(graph_ref), decltype(params)>)
  {
    auto& g = graph_ref(params);
    obj.m_graph_id = g.m_id;
  }

  return obj;
}

// ============================================================================
// gvbars_object - Vertical bar chart
//
// Draws vertical bars from y=0 to the data point.
// ============================================================================

struct gvbars_object : plot_base
{
  double m_delta{1.0}; // Bar width

  // Convenience: plot single bar
  void plot(double x, double y)
  {
    plot_base::plot(x, y);
  }
};

// Parameter specs for gvbars_object (numeric types only)
template<>
struct object_params<gvbars_object>
{
  static constexpr auto value = std::tuple{
    param_spec<&plot_base::m_color, decltype(color), vec3{0, 0, 0}>{},
    param_spec<&plot_base::m_visible, decltype(visible), true>{},
    param_spec<&plot_base::m_legend, decltype(legend), true>{},
    param_spec<&gvbars_object::m_delta, decltype(delta), 1.0>{}
  };
};

// Factory function for gvbars
template<typename... Binders>
gvbars_object gvbars(Binders... binders)
{
  auto params = substitution(binders...);
  gvbars_object obj{};
  obj.m_plot_id = detail::next_plot_id++;

  // Apply numeric parameters
  apply_params(obj, params, object_params<gvbars_object>::value);

  // Handle string parameters manually
  if constexpr (is_bound<decltype(label), decltype(params)>)
    obj.m_label = label(params);

  // Extract graph reference if provided
  if constexpr (is_bound<decltype(graph_ref), decltype(params)>)
  {
    auto& g = graph_ref(params);
    obj.m_graph_id = g.m_id;
  }

  return obj;
}

// ============================================================================
// Plot entry (for type-erased storage in graph_registry)
// ============================================================================

struct plot_entry
{
  plot_type type;
  std::size_t index; // Index into type-specific storage
};

// ============================================================================
// graph_registry - Global storage for graphs and plots
//
// Provides central management for the JS bridge to sync data.
// ============================================================================

class graph_registry
{
public:
  // Graph storage
  std::vector<graph_object> m_graphs;

  // Plot storage (type-specific)
  std::vector<gcurve_object> m_gcurves;
  std::vector<gdots_object> m_gdots;
  std::vector<gvbars_object> m_gvbars;

  // Plot registry (type-erased references)
  std::vector<plot_entry> m_plot_entries;

  // Register a graph
  std::size_t add(graph_object& g)
  {
    std::size_t idx = m_graphs.size();
    m_graphs.push_back(g);
    g.m_id = m_graphs[idx].m_id; // Sync ID
    return idx;
  }

  // Register a gcurve
  std::size_t add(gcurve_object& curve, graph_object& parent)
  {
    curve.m_graph_id = parent.m_id;
    std::size_t idx = m_gcurves.size();
    m_gcurves.push_back(curve);
    m_plot_entries.push_back({plot_type::gcurve, idx});
    parent.register_plot(curve.m_plot_id);
    return idx;
  }

  // Register a gdots
  std::size_t add(gdots_object& dots, graph_object& parent)
  {
    dots.m_graph_id = parent.m_id;
    std::size_t idx = m_gdots.size();
    m_gdots.push_back(dots);
    m_plot_entries.push_back({plot_type::gdots, idx});
    parent.register_plot(dots.m_plot_id);
    return idx;
  }

  // Register a gvbars
  std::size_t add(gvbars_object& bars, graph_object& parent)
  {
    bars.m_graph_id = parent.m_id;
    std::size_t idx = m_gvbars.size();
    m_gvbars.push_back(bars);
    m_plot_entries.push_back({plot_type::gvbars, idx});
    parent.register_plot(bars.m_plot_id);
    return idx;
  }

  // Find graph by ID
  graph_object* find_graph(std::size_t id)
  {
    for (auto& g : m_graphs)
    {
      if (g.m_id == id) return &g;
    }
    return nullptr;
  }

  // Clear all graphs and plots
  void clear()
  {
    m_graphs.clear();
    m_gcurves.clear();
    m_gdots.clear();
    m_gvbars.clear();
    m_plot_entries.clear();
    detail::next_graph_id = 1;
    detail::next_plot_id = 1;
  }
};

// Global graph registry
inline graph_registry g_graphs{};

} // namespace vcpp
