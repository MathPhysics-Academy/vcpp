/*
 *  basic_test.cppm - Test VCpp with WebGPU backend
 */

module;

import std;

export module basic_test;

import vcpp;
import vcpp.wgpu;

export int run_test()
{
  using namespace vcpp;

  // Initialize WebGPU backend with default scene
  if (!wgpu::init(scene))
  {
    std::cerr << "Failed to initialize WebGPU backend\n";
    return 1;
  }

  // Set background color
  scene.background(vec3{0, 0, 0});

  // Create objects
  sphere(pos = vec3{0, 0, 0}, radius = 1.0, color = colors::red);
  sphere(pos = vec3{2.5, 0, 0}, radius = 0.5, color = colors::blue);
  box(pos = vec3{-2.5, 0, 0}, length = 1.0, height = 1.0, width = 1.0, color = colors::green);

  // Animation: rotate objects
  double t = 0.0;

  // Main loop
  loop(
    [&] {
      t += 0.016; // ~60fps

      // Orbit camera slowly
      scene.cam().orbit(0.005, 0.0);

      // Mark scene as needing redraw
      scene.mark_dirty();
    },
    60.0, scene);

  wgpu::shutdown();
  return 0;
}
