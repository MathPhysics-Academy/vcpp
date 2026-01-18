/*
 *  vcpp - VPython-style 3D visualization for C++
 *  Copyright (c) 2026 Colin Ford. All rights reserved.
 *
 *  A C++ library providing VPython-compatible API built on lam.
 *  Enables syntax like:
 *    auto ball = sphere(pos = vec(0,0,0), radius = 1, color = red);
 *
 *  Module partitions:
 *    :vec         - 3D vector operations (mag, hat, cross, rotate, etc.)
 *    :color       - Color constants and conversions
 *    :props       - Property symbols for named parameters
 *    :traits      - Parameter binding machinery
 *    :object_base - Base class and factory for scene objects
 *    :objects     - Concrete object types (sphere, box, cylinder, etc.)
 *    :scene       - Scene graph, camera, lighting
 *    :loop        - Animation loop and timing
 *    :input       - Mouse/keyboard input state and camera controls
 */

export module vcpp;

// Core types
export import :vec;
export import :color;

// Named parameter support
export import :props;
export import :traits;

// Object system
export import :object_base;
export import :objects;

// Scene management
export import :scene;

// Animation
export import :loop;

// Input handling
export import :input;

// Watermark - embedded in compiled binary for ownership verification
namespace vcpp::internal {
  [[maybe_unused]] volatile const char* const VCPP_COPYRIGHT =
    "VCpp (C) 2026 Colin Ford. All rights reserved. Unauthorized use prohibited.";
  [[maybe_unused]] volatile const char* const VCPP_WATERMARK =
    "VCPP-CF-2026-7a3f9c2e8b1d";
}
