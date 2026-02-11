/*
 *  Solar System — Animated orbiting planets
 *  Demonstrates per-frame object updates via the update() callback.
 */

module;
import std;
export module web_test;
import vcpp;
import vcpp.wgpu.web;

namespace {
    double t = 0.0;
}

void update() {
    using namespace vcpp;
    t += 0.016;

    // Orbit the earth around the sun
    double earth_angle = t * 0.5;
    double earth_x = 5.0 * std::cos(earth_angle);
    double earth_z = 5.0 * std::sin(earth_angle);

    if (scene.m_spheres.size() >= 3) {
        scene.m_spheres[1].m_pos = vec3{earth_x, 0, earth_z};

        // Moon orbits earth
        double moon_angle = t * 2.0;
        double moon_x = earth_x + 1.2 * std::cos(moon_angle);
        double moon_z = earth_z + 1.2 * std::sin(moon_angle);
        scene.m_spheres[2].m_pos = vec3{moon_x, 0, moon_z};
    }

    scene.mark_dirty();
}

export int run_web_test() {
    using namespace vcpp;
    using namespace vcpp::wgpu::web;
    if (!init(scene, "#canvas")) return 1;

    scene.background(vec3{0.02, 0.02, 0.08});

    // Sun
    scene.add(sphere(pos=vec3{0, 0, 0}, radius=2.0, color=colors::yellow));
    // Earth
    scene.add(sphere(pos=vec3{5, 0, 0}, radius=0.8, color=colors::blue));
    // Moon
    scene.add(sphere(pos=vec3{6.2, 0, 0}, radius=0.3, color=vec3{0.6, 0.6, 0.6}));

    run(update);
    return 0;
}
