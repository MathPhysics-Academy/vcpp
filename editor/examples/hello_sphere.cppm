/*
 *  Hello Sphere — Minimal VCpp example
 *  Creates a few colored 3D objects in a scene.
 */

module;
import std;
export module web_test;
import vcpp;
import vcpp.wgpu.web;

void update() {
    vcpp::scene.mark_dirty();
}

export int run_web_test() {
    using namespace vcpp;
    using namespace vcpp::wgpu::web;
    if (!init(scene, "#canvas")) return 1;

    scene.background(vec3{0.1, 0.1, 0.15});

    // Create a red sphere
    scene.add(sphere(pos=vec3{0, 0, 0}, radius=1.0, color=colors::red));

    // Add a green box nearby
    scene.add(box(pos=vec3{2.5, 0, 0}, length=1.0, height=1.0, width=1.0, color=colors::green));

    // Add a blue ellipsoid
    scene.add(ellipsoid(pos=vec3{-2.5, 0, 0}, length=2.0, height=0.5, width=0.5, color=colors::blue));

    run(update);
    return 0;
}
