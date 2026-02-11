/*
 *  Bouncing Ball + Energy Graph
 *  Demonstrates physics simulation with real-time 2D graph plotting.
 */

module;
import std;
export module web_test;
import vcpp;
import vcpp.wgpu.web;

namespace {
    double ball_y = 5.0;
    double ball_vy = 0.0;
    double sim_time = 0.0;
    std::size_t ball_idx = 0;

    constexpr double ball_g = 9.8;
    constexpr double ball_mass = 1.0;
    constexpr double restitution = 0.85;
    constexpr double floor_y = 0.0;
    constexpr double ball_radius = 0.5;
    constexpr double sim_dt = 0.01;
}

void update() {
    using namespace vcpp;

    // Physics
    ball_vy -= ball_g * sim_dt;
    ball_y += ball_vy * sim_dt;

    if (ball_y - ball_radius < floor_y) {
        ball_y = floor_y + ball_radius;
        ball_vy = -ball_vy * restitution;
    }

    if (ball_idx < scene.m_spheres.size()) {
        scene.m_spheres[ball_idx].m_pos = vec3{0, ball_y, 0};
    }

    // Energy calculations for graph
    double height = ball_y - ball_radius;
    double ke = 0.5 * ball_mass * ball_vy * ball_vy;
    double pe = ball_mass * ball_g * height;
    double total = ke + pe;

    static int frame_counter = 0;
    if (frame_counter % 3 == 0 && g_graphs.m_gcurves.size() >= 3) {
        g_graphs.m_gcurves[0].plot(sim_time, ke);
        g_graphs.m_gcurves[1].plot(sim_time, pe);
        g_graphs.m_gcurves[2].plot(sim_time, total);

        std::size_t graph_id = g_graphs.m_graphs[0].m_id;
        graph_bridge::queue_point(graph_id, g_graphs.m_gcurves[0].m_plot_id, sim_time, ke);
        graph_bridge::queue_point(graph_id, g_graphs.m_gcurves[1].m_plot_id, sim_time, pe);
        graph_bridge::queue_point(graph_id, g_graphs.m_gcurves[2].m_plot_id, sim_time, total);

        if (!g_graphs.m_graphs.empty())
            g_graphs.m_graphs[0].m_dirty = true;
    }
    frame_counter++;
    sim_time += sim_dt;

    scene.mark_dirty();
}

export int run_web_test() {
    using namespace vcpp;
    using namespace vcpp::wgpu::web;
    if (!init(scene, "#canvas")) return 1;

    scene.background(vec3{0.1, 0.1, 0.15});

    // Floor
    scene.add(box(pos=vec3{0, -0.5, 0}, length=10.0, height=1.0, width=10.0,
                  color=vec3{0.3, 0.3, 0.35}, shininess=0.8));

    // Ball
    scene.add(sphere(pos=vec3{0, ball_y, 0}, radius=ball_radius, color=colors::red));
    ball_idx = scene.m_spheres.size() - 1;

    // Energy graph
    auto energy_graph = graph(
        title="Bouncing Ball Energy",
        xtitle="Time (s)",
        ytitle="Energy (J)",
        width=300, height=200
    );
    g_graphs.add(energy_graph);

    auto ke_curve = gcurve(color=colors::red, prop::label="KE", width=2.0);
    auto pe_curve = gcurve(color=colors::blue, prop::label="PE", width=2.0);
    auto te_curve = gcurve(color=colors::green, prop::label="Total", width=2.0);

    g_graphs.add(ke_curve, g_graphs.m_graphs[0]);
    g_graphs.add(pe_curve, g_graphs.m_graphs[0]);
    g_graphs.add(te_curve, g_graphs.m_graphs[0]);

    graph_bridge::sync_all();

    run(update);
    return 0;
}
