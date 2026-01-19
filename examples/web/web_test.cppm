/*
 *  web_test.cppm - Browser WebGPU test
 */

module;

import std;

export module web_test;

import vcpp;
import vcpp.wgpu.web;

namespace
{
double t = 0.0;
}

void update()
{
  using namespace vcpp;
  t += 0.016;
  // Camera is now controlled by mouse:
  // - Left-drag: orbit
  // - Right-drag or Ctrl+Left-drag: pan
  // - Scroll wheel: zoom
}

export int run_web_test()
{
  using namespace vcpp;
  using namespace vcpp::wgpu::web;

  // Initialize WebGPU backend
  if (!init(scene, "#canvas"))
  {
    return 1;
  }

  // Set background color
  scene.background(vec3{0, 0, 0});

  // Create objects
  sphere_object s1 = sphere(pos = vec3{0, 0, 0}, radius = 1.0, color = colors::red);
  sphere_object s2 = sphere(pos = vec3{2.5, 0, 0}, radius = 0.5, color = colors::blue);
  box_object b1 = box(pos = vec3{-2.5, 0, 0}, length = 1.0, height = 1.0, width = 1.0, color = colors::green);

  // Requested Red Ellipsoid
  ellipsoid_object e1 = ellipsoid(pos = vec3{0, 2.5, 0}, length = 2.0, height = 0.5, width = 0.5, axis = vec3{1, 0, 0},
                                  color = colors::red);

  scene.add(s1);
  scene.add(s2);
  scene.add(b1);
  scene.add(e1);

  // Start main loop (this doesn't return in browser)
  run(update);

  return 0;
}
