/*
 *  vcpp:graph_base - Base structs for 2D graphing
 *
 *  Provides:
 *  - graph_base: Container for plots (like VPython's graph)
 *  - plot_base: Data series within a graph
 *
 *  Graph objects are 2D, not 3D scene objects - they have their own
 *  base classes that don't inherit from object_base.
 */

module;

import std;

export module vcpp:graph_base;

import :vec;

export namespace vcpp
{

// ============================================================================
// graph_base - Container for 2D plots
//
// Manages canvas properties, axis configuration, and auto-scaling.
// Equivalent to VPython's graph() object.
// ============================================================================

struct graph_base
{
  std::size_t m_id{0};  // Unique identifier for JS bridge

  // Canvas dimensions
  int m_width{640};
  int m_height{400};

  // Colors (RGB normalized 0-1)
  vec3 m_background{0.1, 0.1, 0.15};  // Dark theme default
  vec3 m_foreground{0.9, 0.9, 0.9};   // Light text/axes

  // Labels
  std::string m_title{};
  std::string m_xtitle{};
  std::string m_ytitle{};

  // Axis ranges (0 = auto-scale)
  double m_xmin{0};
  double m_xmax{0};
  double m_ymin{0};
  double m_ymax{0};

  // Flags
  bool m_fast{true};     // Prioritize performance over quality
  bool m_visible{true};  // Display toggle

  // Computed auto-scale bounds (updated during data updates)
  double m_auto_xmin{0};
  double m_auto_xmax{1};
  double m_auto_ymin{0};
  double m_auto_ymax{1};

  // Dirty flag for JS sync
  bool m_dirty{true};

  // Get effective axis bounds (manual or auto)
  double effective_xmin() const noexcept { return m_xmin != 0 || m_xmax != 0 ? m_xmin : m_auto_xmin; }
  double effective_xmax() const noexcept { return m_xmin != 0 || m_xmax != 0 ? m_xmax : m_auto_xmax; }
  double effective_ymin() const noexcept { return m_ymin != 0 || m_ymax != 0 ? m_ymin : m_auto_ymin; }
  double effective_ymax() const noexcept { return m_ymin != 0 || m_ymax != 0 ? m_ymax : m_auto_ymax; }
};

// ============================================================================
// plot_base - Data series within a graph
//
// Base class for gcurve, gdots, gvbars. Manages data points and styling.
// ============================================================================

struct plot_base
{
  std::size_t m_graph_id{0};  // Parent graph ID
  std::size_t m_plot_id{0};   // Unique plot ID within graph

  // Data points (x, y pairs)
  std::vector<std::pair<double, double>> m_data;

  // Appearance
  vec3 m_color{0, 0, 0};  // Black default (shows on white/light backgrounds)
  std::string m_label{};  // Legend label

  // Flags
  bool m_visible{true};
  bool m_legend{true};  // Show in legend

  // Dirty tracking for incremental updates
  std::size_t m_last_synced{0};  // Index of last point sent to JS
  bool m_dirty{true};

  // Add a single data point
  void plot(double x, double y)
  {
    m_data.emplace_back(x, y);
    m_dirty = true;
  }

  // Add multiple data points
  void plot(const std::vector<std::pair<double, double>>& points)
  {
    m_data.insert(m_data.end(), points.begin(), points.end());
    m_dirty = true;
  }

  // Clear all data
  void clear_data()
  {
    m_data.clear();
    m_last_synced = 0;
    m_dirty = true;
  }

  // Get number of new points since last sync
  std::size_t new_points_count() const noexcept
  {
    return m_data.size() > m_last_synced ? m_data.size() - m_last_synced : 0;
  }
};

// ============================================================================
// Plot type enumeration (for type-erased storage)
// ============================================================================

enum class plot_type
{
  gcurve,  // Connected line plot
  gdots,   // Scatter plot
  gvbars   // Vertical bar chart
};

} // namespace vcpp
