/*
 *  vcpp:props - Property symbols for VPython-style named parameters
 *
 *  Uses lam::symbols binding mechanism to enable syntax like:
 *    sphere(pos = vec(0,0,0), radius = 1, color = red)
 */

module;

import std;

export module vcpp:props;

import lam.symbols;

export namespace vcpp
{

using lam::symbols::symbol;
using lam::symbols::unconstrained;

// ============================================================================
// Common Object Properties
// ============================================================================

// Spatial
inline constexpr symbol pos{};  // position (vec3)
inline constexpr symbol axis{}; // orientation/direction (vec3)
inline constexpr symbol up{};   // up direction (vec3)
inline constexpr symbol size{}; // bounding size (vec3)

// Appearance
inline constexpr symbol color{};     // RGB color (vec3)
inline constexpr symbol opacity{};   // transparency 0-1 (double)
inline constexpr symbol shininess{}; // reflectivity 0-1 (double)
inline constexpr symbol emissive{};  // self-illumination (bool)
inline constexpr symbol visible{};   // display toggle (bool)
inline constexpr symbol texture{};   // texture reference

// Behavior
inline constexpr symbol make_trail{};   // leave trail when moved (bool)
inline constexpr symbol retain{};       // trail retain time (double, -1 = infinite)
inline constexpr symbol trail_color{};  // trail color (vec3)
inline constexpr symbol trail_radius{}; // trail thickness (double)

// ============================================================================
// Geometry Properties
// ============================================================================

// General dimensions
inline constexpr symbol radius{};    // sphere, cylinder, cone, ring, helix
inline constexpr symbol length{};    // box, cylinder, cone
inline constexpr symbol height{};    // box
inline constexpr symbol width{};     // box
inline constexpr symbol thickness{}; // ring, helix

// ============================================================================
// Arrow-specific Properties
// ============================================================================

inline constexpr symbol shaftwidth{};
inline constexpr symbol headwidth{};
inline constexpr symbol headlength{};
inline constexpr symbol round{}; // round vs square shaft (bool)

// ============================================================================
// Helix-specific Properties
// ============================================================================

inline constexpr symbol coils{}; // number of coils (int/double)
inline constexpr symbol ccw{};   // counter-clockwise winding (bool)

// ============================================================================
// Curve/Path Properties
// ============================================================================

// inline constexpr symbol points{}; // MOVED TO vcpp::prop - conflicts with points() factory
inline constexpr symbol closed{}; // closed path (bool)

// ============================================================================
// Text/Label Properties
// ============================================================================

inline constexpr symbol text{};       // text content (string)
inline constexpr symbol font{};       // font name (string)
inline constexpr symbol billboard{};  // always face camera (bool)
inline constexpr symbol xoffset{};    // x offset from pos (double)
inline constexpr symbol yoffset{};    // y offset from pos (double)
inline constexpr symbol depth{};      // extrusion depth for 3D text (double)
inline constexpr symbol align{};      // text alignment: left, center, right (string)
inline constexpr symbol border{};     // border padding for labels (double)
// inline constexpr symbol box{};        // MOVED TO vcpp::prop - conflicts with box() factory
inline constexpr symbol line{};       // draw line from pos to label (bool)

// ============================================================================
// Extrusion Properties
// ============================================================================

inline constexpr symbol path{};            // extrusion path (vector<vec3>)
inline constexpr symbol shape{};           // 2D cross-section (vector<vec2>)
inline constexpr symbol twist{};           // total twist angle (double)
inline constexpr symbol scale_end{};       // end scale factor (double)
inline constexpr symbol show_start_face{}; // cap at start (bool)
inline constexpr symbol show_end_face{};   // cap at end (bool)

// ============================================================================
// Vertex/Triangle/Quad Properties
// ============================================================================

inline constexpr symbol v0{};      // first vertex
inline constexpr symbol v1{};      // second vertex
inline constexpr symbol v2{};      // third vertex
inline constexpr symbol v3{};      // fourth vertex (quad)
inline constexpr symbol normal{};  // vertex normal
inline constexpr symbol texpos{};  // texture coordinates

// ============================================================================
// Scene/Canvas Properties
// ============================================================================

inline constexpr symbol background{}; // canvas background color (vec3)
inline constexpr symbol center{};     // camera look-at point (vec3)
inline constexpr symbol forward{};    // camera direction (vec3)
inline constexpr symbol range{};      // visible range (double)
inline constexpr symbol fov{};        // field of view in degrees (double)
inline constexpr symbol lights{};     // light sources
inline constexpr symbol ambient{};    // ambient light level (vec3)
inline constexpr symbol caption{};    // canvas caption (string)
inline constexpr symbol title{};      // canvas title (string)

// ============================================================================
// Animation/Physics Properties
// ============================================================================

inline constexpr symbol velocity{};     // for physics simulations (vec3)
inline constexpr symbol acceleration{}; // (vec3)
inline constexpr symbol momentum{};     // (vec3)
inline constexpr symbol mass{};         // (double)
inline constexpr symbol charge{};       // (double)
inline constexpr symbol dt{};           // time step (double)

// ============================================================================
// Object Management
// ============================================================================

inline constexpr symbol target_canvas{}; // target canvas for rendering
inline constexpr symbol group{};         // group membership

// ============================================================================
// Graph Properties (for 2D plotting)
// ============================================================================

// Graph container properties
inline constexpr symbol xtitle{};     // X-axis label (string)
inline constexpr symbol ytitle{};     // Y-axis label (string)
inline constexpr symbol xmin{};       // X-axis minimum (double, 0 = auto)
inline constexpr symbol xmax{};       // X-axis maximum (double, 0 = auto)
inline constexpr symbol ymin{};       // Y-axis minimum (double, 0 = auto)
inline constexpr symbol ymax{};       // Y-axis maximum (double, 0 = auto)
inline constexpr symbol fast{};       // Performance mode (bool)
inline constexpr symbol foreground{}; // Foreground/text color (vec3)

// Plot properties
inline constexpr symbol graph_ref{};    // Reference to parent graph
// inline constexpr symbol label{};        // MOVED TO vcpp::prop - conflicts with label() factory
inline constexpr symbol legend{};       // Show in legend (bool)
inline constexpr symbol show_dot{};     // Show markers on gcurve (bool)
inline constexpr symbol dot_color{};    // Marker color for gcurve (vec3)
inline constexpr symbol dot_radius{};   // Marker radius for gcurve (double)
inline constexpr symbol markers{};      // Marker style (bool)
inline constexpr symbol marker_radius{};// Marker radius (double)
inline constexpr symbol delta{};        // Bar width for gvbars (double)

} // namespace vcpp

// ============================================================================
// Conflicting Property Symbols
// These are in a separate namespace to avoid collision with factory functions.
// Use vcpp::prop::box, vcpp::prop::points, vcpp::prop::label
// ============================================================================

export namespace vcpp::prop
{

using lam::symbols::symbol;

inline constexpr symbol box{};    // draw background box for labels (bool)
inline constexpr symbol points{}; // list of points for curves
inline constexpr symbol label{};  // legend label (string)

} // namespace vcpp::prop
