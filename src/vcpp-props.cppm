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

export namespace vcpp {

using lam::symbols::symbol;
using lam::symbols::unconstrained;

// ============================================================================
// Common Object Properties
// ============================================================================

// Spatial
inline constexpr symbol pos{};            // position (vec3)
inline constexpr symbol axis{};           // orientation/direction (vec3)
inline constexpr symbol up{};             // up direction (vec3)
inline constexpr symbol size{};           // bounding size (vec3)

// Appearance
inline constexpr symbol color{};          // RGB color (vec3)
inline constexpr symbol opacity{};        // transparency 0-1 (double)
inline constexpr symbol shininess{};      // reflectivity 0-1 (double)
inline constexpr symbol emissive{};       // self-illumination (bool)
inline constexpr symbol visible{};        // display toggle (bool)
inline constexpr symbol texture{};        // texture reference

// Behavior
inline constexpr symbol make_trail{};     // leave trail when moved (bool)
inline constexpr symbol retain{};         // trail retain time (double, -1 = infinite)
inline constexpr symbol trail_color{};    // trail color (vec3)
inline constexpr symbol trail_radius{};   // trail thickness (double)

// ============================================================================
// Geometry Properties
// ============================================================================

// General dimensions
inline constexpr symbol radius{};         // sphere, cylinder, cone, ring, helix
inline constexpr symbol length{};         // box, cylinder, cone
inline constexpr symbol height{};         // box
inline constexpr symbol width{};          // box
inline constexpr symbol thickness{};      // ring, helix

// ============================================================================
// Arrow-specific Properties
// ============================================================================

inline constexpr symbol shaftwidth{};
inline constexpr symbol headwidth{};
inline constexpr symbol headlength{};
inline constexpr symbol round{};          // round vs square shaft (bool)

// ============================================================================
// Helix-specific Properties
// ============================================================================

inline constexpr symbol coils{};          // number of coils (int/double)
inline constexpr symbol ccw{};            // counter-clockwise winding (bool)

// ============================================================================
// Curve/Path Properties
// ============================================================================

inline constexpr symbol points{};         // list of points
inline constexpr symbol closed{};         // closed path (bool)

// ============================================================================
// Text/Label Properties
// ============================================================================

inline constexpr symbol text{};           // text content (string)
inline constexpr symbol font{};           // font name (string)
inline constexpr symbol billboard{};      // always face camera (bool)
inline constexpr symbol xoffset{};        // x offset from pos (double)
inline constexpr symbol yoffset{};        // y offset from pos (double)

// ============================================================================
// Scene/Canvas Properties
// ============================================================================

inline constexpr symbol background{};     // canvas background color (vec3)
inline constexpr symbol center{};         // camera look-at point (vec3)
inline constexpr symbol forward{};        // camera direction (vec3)
inline constexpr symbol range{};          // visible range (double)
inline constexpr symbol fov{};            // field of view in degrees (double)
inline constexpr symbol lights{};         // light sources
inline constexpr symbol ambient{};        // ambient light level (vec3)
inline constexpr symbol caption{};        // canvas caption (string)
inline constexpr symbol title{};          // canvas title (string)

// ============================================================================
// Animation/Physics Properties
// ============================================================================

inline constexpr symbol velocity{};       // for physics simulations (vec3)
inline constexpr symbol acceleration{};   // (vec3)
inline constexpr symbol momentum{};       // (vec3)
inline constexpr symbol mass{};           // (double)
inline constexpr symbol charge{};         // (double)
inline constexpr symbol dt{};             // time step (double)

// ============================================================================
// Object Management
// ============================================================================

inline constexpr symbol target_canvas{};  // target canvas for rendering
inline constexpr symbol group{};          // group membership

} // namespace vcpp
