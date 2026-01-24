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
  
  // Raindrop simulation state
  struct drop_state {
    std::size_t index; // Index into scene.m_ellipsoids
    vcpp::vec3 velocity{0, 0, 0};
    vcpp::vec3 acceleration{0, 0, 0};
    bool is_splatting{false}; // New: Animation state
  };
  
  std::vector<drop_state> drops;
  bool is_raining_scene = false;
  
  enum class SceneID { Default, Solar, Random, Rain, Torque, Graph };
  SceneID current_scene = SceneID::Default;

  // Graph demo state
  struct graph_demo_state {
    double ball_y{5.0};
    double ball_vy{0.0};
    double time{0.0};
    std::size_t ball_idx{0};
    bool active{false};
  } g_demo;

  // Physics constants
  constexpr double sim_dt = 0.01;
  constexpr double gravity = -9.8;
  constexpr double drop_constant = 0.5;
  constexpr double drop_power = 2.0; // Check strict typing
  constexpr int Ndrops = 100;
  constexpr double dropheight = 4.0;

  // Torque Simulation State
  struct torque_state {
      double theta{0.0};
      double omega{0.0};
      // Fixed geometry reference (unrotated)
      vcpp::vec3 corners[4];
      vcpp::vec3 axes[4];
      // Object indices
      std::size_t loop_idxs[4];
      std::size_t force_idxs[4];
      std::size_t torque_idx;
      
      bool active{false};
  } t_sim;

  // Helper: Rotate vector around Y axis
  vcpp::vec3 rotate_y(vcpp::vec3 v, double angle) {
      double c = std::cos(angle);
      double s = std::sin(angle);
      return vcpp::vec3{v.x()*c + v.z()*s, v.y(), -v.x()*s + v.z()*c};
  }


  // Scene ID tracking already defined above

  void load_scene_default() {
    current_scene = SceneID::Default;
    using namespace vcpp;
    scene.clear();
    graph_bridge::clear_all();  // Clear any graphs
    is_raining_scene = false;
    t_sim.active = false;
    g_demo.active = false;
    scene.background(vec3{0, 0, 0});
    
    // Default: 2 Spheres, 1 Cube, 1 Ellipsoid
    sphere_object s1 = sphere(pos=vec3{0, 0, 0}, radius=1.0, color=colors::red);
    sphere_object s2 = sphere(pos=vec3{2.5, 0, 0}, radius=0.5, color=colors::blue);
    box_object b1 = box(pos=vec3{-2.5, 0, 0}, length=1.0, height=1.0, width=1.0, color=colors::green);
    ellipsoid_object e1 = ellipsoid(pos=vec3{0, 2.5, 0}, length=2.0, height=0.5, width=0.5, axis=vec3{1,0,0}, color=colors::red);

    scene.add(s1);
    scene.add(s2);
    scene.add(b1);
    scene.add(e1);
    std::cout << "Loaded Default Scene" << std::endl;
  }

  void load_scene_solar() {
    current_scene = SceneID::Solar;
    using namespace vcpp;
    scene.clear();
    graph_bridge::clear_all();
    is_raining_scene = false;
    t_sim.active = false;
    g_demo.active = false;
    scene.background(vec3{0.05, 0.05, 0.1}); // Starry dark blue
    
    // Sun
    scene.add(sphere(pos=vec3{0,0,0}, radius=2.0, color=colors::yellow));
    // Earth
    scene.add(sphere(pos=vec3{5,0,0}, radius=0.8, color=colors::blue));
    // Moon
    scene.add(sphere(pos=vec3{6,0,0}, radius=0.3, color=vec3{0.6, 0.6, 0.6}));
    
    std::cout << "Loaded Solar System" << std::endl;
  }

  // ==========================================================================
  // SCENE 5: Magnetic Torque on Current Loop (Ported from VPython)
  // ==========================================================================
  void load_scene_torque() {
    current_scene = SceneID::Torque;
    using namespace vcpp;
    scene.clear();
    graph_bridge::clear_all();
    is_raining_scene = false;
    t_sim.active = true;
    g_demo.active = false;
    scene.background(vec3{1, 1, 1}); // White background
    
    // ... rest of torque scene ...
    // B field parameters
    double Bshaft = 0.1; 
    double Bsize = -2.0;
    vec3 Bdirect = vec3{0, 0, Bsize};
    vec3 inc1 = vec3{2.5, 0, 0};
    vec3 inc2 = vec3{0, 2.5, 0};
    
    auto add_B_arrow = [&](vec3 p) {
      scene.add(arrow(pos=p, axis=Bdirect, shaftwidth=Bshaft, color=colors::blue));
    };
    
    vec3 base_pos = vec3{0, 0, -Bsize/2.0};
    add_B_arrow(base_pos); 
    add_B_arrow(base_pos - inc1);
    add_B_arrow(base_pos + inc1);
    add_B_arrow(base_pos - inc2);
    add_B_arrow(base_pos - inc1 - inc2);
    add_B_arrow(base_pos + inc1 - inc2);
    add_B_arrow(base_pos + inc2);
    add_B_arrow(base_pos - inc1 + inc2);
    add_B_arrow(base_pos + inc1 + inc2);
    
    // Initialize Physics State
    double Ishaft = 0.1; 
    t_sim = torque_state{}; 
    t_sim.active = true;
    t_sim.theta = 0.785; 
    t_sim.omega = 0.0;
    
    t_sim.corners[0] = vec3{-1.0, -0.75, 0.0};
    t_sim.axes[0]    = vec3{2.0, 0.0, 0.0};   
    t_sim.corners[1] = vec3{1.0, -0.75, 0.0};
    t_sim.axes[1]    = vec3{0.0, 1.5, 0.0};   
    t_sim.corners[2] = vec3{1.0, 0.75, 0.0};
    t_sim.axes[2]    = vec3{-2.0, 0.0, 0.0};  
    t_sim.corners[3] = vec3{-1.0, 0.75, 0.0};
    t_sim.axes[3]    = vec3{0.0, -1.5, 0.0};  
    
    for(int i=0; i<4; ++i) {
        t_sim.loop_idxs[i] = scene.add(arrow(pos=t_sim.corners[i], axis=t_sim.axes[i], shaftwidth=Ishaft));
    }

    for(int i=0; i<4; ++i) {
        t_sim.force_idxs[i] = scene.add(arrow(pos=vec3{0,0,0}, axis=vec3{0,0,0}, color=colors::red));
    }
    
    t_sim.torque_idx = scene.add(arrow(pos=vec3{0,0,0}, axis=vec3{0,0,0}, color=colors::yellow));
    // The B-field is vertical (Y-axis).
    // A solenoid aligned with Y creates this field.
    // Let's place a large, subtle helix surrounding the loop to show the "source" of the B-field.
    // Loop is roughly at origin, size ~2x1.5.
    // Solenoid should be larger, e.g., radius 3.0, height 8.0?
    
    // Axis: Along Y (-4 to +4)
    // Coils: 10
    // Color: Cyan (matching B-field arrows) but transparent/darker
    scene.add(helix(
        pos=vec3{0, -4, 0}, 
        axis=vec3{0, 8, 0}, 
        radius=3.0, 
        thickness=0.05, 
        coils=10, 
        color=vec3{0.0, 0.5, 0.5}, // Cyan (Source of B-field)
        opacity=0.3 // Transparent so it doesn't block view
    ));
    
    std::cout << "Loaded Magnetic Torque Scene (Animated)" << std::endl;
    std::cout << "Legend:" << std::endl;
    std::cout << "  CYAN Arrows:   Magnetic Field (B)" << std::endl;
    std::cout << "  CYAN Helix:    Solenoid (Source of B)" << std::endl;
    std::cout << "  WHITE Frame:   Current Loop (I)" << std::endl;
    std::cout << "  RED Arrows:    Magnetic Forces (F = I x B)" << std::endl;
    std::cout << "  YELLOW Arrow:  Net Torque (Tau = r x F)" << std::endl;
    std::cout << "Animation: Simple Harmonic Motion (Oscillation)" << std::endl;
  }
  
  void add_random_objects() {
      current_scene = SceneID::Random;
      using namespace vcpp;
      // Make this a standalone scene
      scene.clear();
      graph_bridge::clear_all();
      is_raining_scene = false;
      t_sim.active = false;
      g_demo.active = false;
      scene.background(vec3{0.1, 0.1, 0.1});

      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_real_distribution<> pos_dist(-5.0, 5.0);
      std::uniform_real_distribution<> radius_dist(0.2, 0.8);
      std::uniform_real_distribution<> color_dist(0.0, 1.0);

      for (int i = 0; i < 20; ++i) { // Increased count
        scene.add(sphere(
          pos = vec3{pos_dist(gen), pos_dist(gen), pos_dist(gen)},
          radius = radius_dist(gen),
          color = vec3{color_dist(gen), color_dist(gen), color_dist(gen)}
        ));
      }
      std::cout << "Loaded Random Scene" << std::endl;
  }

  void load_scene_rain() {
    current_scene = SceneID::Rain;
    using namespace vcpp;
    scene.clear();
    graph_bridge::clear_all();

    drops.clear();
    is_raining_scene = true;
    t_sim.active = false;
    g_demo.active = false;
    
    scene.background(vec3{0.8, 0.8, 0.9}); // Light blue sky
    
    // World setup
    double worldsize = 20.0; // Finite platform (User requested "less infinite")
    double max_dropsize = 1.0;
    constexpr int Ndrops = 100;
    constexpr double spawn_area = 15.0; // Keep drops concentrated in center
    double allowed = spawn_area / 2.0 - max_dropsize; // Spawn range
    
    // Floor (Paper thin, practically a plane)
    constexpr double floor_h = 0.001;
    // Dark Green Floor
    scene.add(box(length=worldsize, height=floor_h, width=worldsize, pos=vec3{0, -dropheight, 0}, color=vec3{0.15, 0.2, 0.15}));
    
    // Random generators
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> size_dist(0.3, max_dropsize); // Min size 0.3 to be visible
    std::uniform_real_distribution<> pos_dist(-allowed, allowed);
    
    for(int i=0; i<Ndrops; ++i) {
      double size = size_dist(gen);
      
      // Create ellipsoid drop (Water Blue)
      scene.add(ellipsoid(length=size, height=size, width=size, color=vec3{0.4, 0.6, 1.0}));
      
      // Get the correct index in the type-specific vector
      std::size_t ellipsoid_idx = scene.m_ellipsoids.size() - 1;
      
      // Initial state
      drop_state d;
      d.index = ellipsoid_idx;
      d.velocity = vec3{0, 0, 0};
      d.acceleration = vec3{0, gravity, 0};
      
      // Position ensuring no overlap
      vec3 pos = vec3{pos_dist(gen), dropheight, pos_dist(gen)};
      scene.m_ellipsoids[ellipsoid_idx].m_pos = pos;
      
      drops.push_back(d);
    }
    std::cout << "Loaded Raindrop Physics Scene" << std::endl;
  }

  // ==========================================================================
  // SCENE 6: Bouncing Ball with Real-Time Energy Graphs
  // ==========================================================================
  void load_scene_graph() {
    current_scene = SceneID::Graph;
    using namespace vcpp;
    scene.clear();
    is_raining_scene = false;
    t_sim.active = false;

    // Clear any existing graphs
    graph_bridge::clear_all();

    scene.background(vec3{0.1, 0.1, 0.15});

    // Create floor
    scene.add(box(pos=vec3{0, -0.5, 0}, length=10.0, height=1.0, width=10.0,
                  color=vec3{0.3, 0.3, 0.35}, shininess=0.8));

    // Create bouncing ball
    g_demo = graph_demo_state{};
    g_demo.ball_y = 5.0;
    g_demo.ball_vy = 0.0;
    g_demo.time = 0.0;
    g_demo.active = true;

    scene.add(sphere(pos=vec3{0, g_demo.ball_y, 0}, radius=0.5, color=colors::red));
    g_demo.ball_idx = scene.m_spheres.size() - 1;

    // Create energy graph
    auto energy_graph = graph(
      title="Bouncing Ball Energy",
      xtitle="Time (s)",
      ytitle="Energy (J)",
      width=300,
      height=200
    );
    g_graphs.add(energy_graph);

    // Create curves for KE, PE, and Total Energy
    auto ke_curve = gcurve(color=colors::red, label="KE", width=2.0);
    auto pe_curve = gcurve(color=colors::blue, label="PE", width=2.0);
    auto te_curve = gcurve(color=colors::green, label="Total", width=2.0);

    g_graphs.add(ke_curve, g_graphs.m_graphs[0]);
    g_graphs.add(pe_curve, g_graphs.m_graphs[0]);
    g_graphs.add(te_curve, g_graphs.m_graphs[0]);

    // Sync graphs to JavaScript
    graph_bridge::sync_all();

    std::cout << "Loaded Graph Demo Scene (Press 6)" << std::endl;
    std::cout << "  Red ball bounces with energy loss" << std::endl;
    std::cout << "  Graph shows: KE (red), PE (blue), Total (green)" << std::endl;
  }
}

void update() {
  using namespace vcpp;
  t += 0.016;

  // Scene Switching
  if (g_input.consume_key("Digit1")) { std::cout << "Key 1 -> Default" << std::endl; load_scene_default(); }
  if (g_input.consume_key("Digit2")) { std::cout << "Key 2 -> Solar" << std::endl; load_scene_solar(); }
  if (g_input.consume_key("Digit3")) { std::cout << "Key 3 -> Random" << std::endl; add_random_objects(); }
  if (g_input.consume_key("Digit4")) { std::cout << "Key 4 -> Rain" << std::endl; load_scene_rain(); }
  if (g_input.consume_key("Digit5")) { std::cout << "Key 5 -> Torque" << std::endl; load_scene_torque(); }
  if (g_input.consume_key("Digit6")) { std::cout << "Key 6 -> Graph Demo" << std::endl; load_scene_graph(); }

  if (g_input.consume_key("KeyR")) {
    std::cout << "Resetting Scene..." << std::endl;
    switch(current_scene) {
      case SceneID::Default: load_scene_default(); break;
      case SceneID::Solar: load_scene_solar(); break;
      case SceneID::Random: add_random_objects(); break;
      case SceneID::Rain: load_scene_rain(); break;
      case SceneID::Torque: load_scene_torque(); break;
      case SceneID::Graph: load_scene_graph(); break;
    }
  }
  
  // Physics Update
  if (t_sim.active) {
      using namespace vcpp;
      // Physics (Simple Harmonic Motion for Dipole in B-field)
      // Torque = mu x B.  mu is along normal (rotated X axis). B is Z.
      // tau = |mu||B|sin(theta). Restoring towards B?
      // If current is CCW, mu is along +Z (at theta=0).
      // If B is +Z (actually Bsize=-2 so B is -Z).
      // So mu wants to align with B (-Z). Stable point is 180 deg?
      // Let's just simulate the restoring force towards 0 for visual simplicity.
      
      double alpha = -5.0 * std::sin(t_sim.theta); // Restoring force
      t_sim.omega += alpha * sim_dt;
      t_sim.theta += t_sim.omega * sim_dt;
      t_sim.omega *= 0.995; // Damping
      
      // Update Objects
      vec3 Bdirect = vec3{0, 0, -2.0};
      double current = 0.5; // Scale factor for visuals
      
      for(int i=0; i<4; ++i) {
          // Rotate reference geometry
          vec3 p = rotate_y(t_sim.corners[i], t_sim.theta);
          vec3 ax = rotate_y(t_sim.axes[i], t_sim.theta);
          
          // Update Loop Segment
          if (t_sim.loop_idxs[i] < scene.m_arrows.size()) {
              scene.m_arrows[t_sim.loop_idxs[i]].m_pos = p;
              scene.m_arrows[t_sim.loop_idxs[i]].m_axis = ax;
          }
          
          // Update Force
          // F = I * (L x B)
          vec3 F = cross(ax, Bdirect) * current;
          if (t_sim.force_idxs[i] < scene.m_arrows.size()) {
              scene.m_arrows[t_sim.force_idxs[i]].m_pos = p + ax * 0.5; // Center of segment
              scene.m_arrows[t_sim.force_idxs[i]].m_axis = F;
          }
      }
      
      // Update Torque Vector?
      // Torque is sum(r x F).
      // Or just visualize total torque on the system (Y-axis vector).
      // tau_mag = -sin(theta). Vector along Y.
      vec3 tau_vec = vec3{0, alpha * 0.5, 0}; // Visual scale
      if (t_sim.torque_idx < scene.m_arrows.size()) {
          scene.m_arrows[t_sim.torque_idx].m_axis = tau_vec;
          // Position it above?
           scene.m_arrows[t_sim.torque_idx].m_pos = vec3{0, 2.0, 0};
      }
      
      scene.mark_dirty();
  }
  else if (is_raining_scene) {
    for (auto& drop : drops) {
      // Safety check
      if (drop.index >= scene.m_ellipsoids.size()) continue;

      auto& obj = scene.m_ellipsoids[drop.index];
      
      // Update position (if not splatting)
      if (!drop.is_splatting) {
        obj.m_pos = obj.m_pos + drop.velocity * sim_dt;
      }
      
      // Check collision or Splat Animation
      double floor_half_height = 0.0005; // 0.001 / 2
      double collision_epsilon = 0.005; // Visual offset bias (5mm)
      double floor_top_y = -dropheight + floor_half_height + collision_epsilon;
      
      double drop_half_height = obj.m_height * 0.5;
      
      bool hitting_floor = (obj.m_pos.y() - drop_half_height <= floor_top_y);
      
      if (drop.is_splatting || hitting_floor) {
         if (!drop.is_splatting) {
           // First frame of impact
           drop.is_splatting = true;
           drop.velocity = vec3{drop.velocity.x(), 0.0, drop.velocity.z()};
           // Snap strictly to touch point to avoid overshooting
           obj.m_pos = vec3{obj.m_pos.x(), floor_top_y + drop_half_height, obj.m_pos.z()};
         }
         
         // Animate flattening!
         // Target: Bead shape (User wants flatter: 0.25 of radius)
         double vol = obj.m_width * obj.m_width * obj.m_height;
         double original_radius = std::cbrt(vol);
         double target_h = original_radius * 0.25; // Flatter (Pancake/Bead hybrid)
         if (target_h < 0.02) target_h = 0.02;     // Absolute min thickness
         
         // Smooth damp height towards target
         double current_h = obj.m_height;
         // Slower animation (3.0 factor) to be visible
         double smooth_factor = 3.0 * sim_dt; 
         if (smooth_factor > 1.0) smooth_factor = 1.0;
         
         double new_h = current_h + (target_h - current_h) * smooth_factor;
         
         // Apply new dimensions (Volume conservation)
         obj.m_height = new_h;
         double w_new = std::sqrt(vol / new_h);
         obj.m_width = w_new;
         obj.m_length = w_new;
         
         // Anchor to floor (Bottom of drop = Top of floor)
         // pos.y = floor_top + new_h / 2
         obj.m_pos = vec3{obj.m_pos.x(), floor_top_y + new_h * 0.5, obj.m_pos.z()};

      } else {
        // Free fall physics (Drag etc)
        // ... (Keep existing drag logic)
        double vol = obj.m_width * obj.m_width * obj.m_height;
        double accel_drag = 0;
        if (vol > 0.0001) {
             double sign_v = (drop.velocity.y() > 0) ? 1.0 : -1.0;
             double v_abs = std::abs(drop.velocity.y());
             
             accel_drag = sign_v * drop_constant * (obj.m_width * obj.m_width) * std::pow(v_abs, drop_power);
             accel_drag /= vol; 
        }
        
        // Update acceleration.y
        drop.acceleration = vec3{drop.acceleration.x(), gravity - accel_drag, drop.acceleration.z()};
        
        // Update velocity.y
        double new_vy = drop.velocity.y() + drop.acceleration.y() * sim_dt;
        drop.velocity = vec3{drop.velocity.x(), new_vy, drop.velocity.z()};
        
        // Air deformation (Aspect Ratio based)
        double AR = 1.0 - 0.02 * std::abs(drop.velocity.y());
        if (AR < 0.3) AR = 0.3;
        
        double w_new = std::cbrt(vol / AR);
        obj.m_width = w_new;
        obj.m_length = w_new;
        obj.m_height = w_new * AR;
      }
    }
    scene.mark_dirty();
  }
  else if (g_demo.active) {
    // Bouncing ball physics with energy graphing
    constexpr double ball_g = 9.8;
    constexpr double ball_mass = 1.0;
    constexpr double restitution = 0.85; // Energy loss on bounce
    constexpr double floor_y = 0.0;
    constexpr double ball_radius = 0.5;

    // Update velocity and position
    g_demo.ball_vy -= ball_g * sim_dt;
    g_demo.ball_y += g_demo.ball_vy * sim_dt;

    // Check for floor collision
    if (g_demo.ball_y - ball_radius < floor_y) {
      g_demo.ball_y = floor_y + ball_radius;
      g_demo.ball_vy = -g_demo.ball_vy * restitution;
    }

    // Update ball position in scene
    if (g_demo.ball_idx < scene.m_spheres.size()) {
      scene.m_spheres[g_demo.ball_idx].m_pos = vec3{0, g_demo.ball_y, 0};
    }

    // Calculate energies
    double height = g_demo.ball_y - ball_radius; // Height above floor
    double ke = 0.5 * ball_mass * g_demo.ball_vy * g_demo.ball_vy;
    double pe = ball_mass * ball_g * height;
    double total = ke + pe;

    // Plot data points (every few frames to avoid overwhelming the graph)
    static int frame_counter = 0;
    if (frame_counter % 3 == 0 && g_graphs.m_gcurves.size() >= 3) {
      g_graphs.m_gcurves[0].plot(g_demo.time, ke);
      g_graphs.m_gcurves[1].plot(g_demo.time, pe);
      g_graphs.m_gcurves[2].plot(g_demo.time, total);

      // Queue points for JS bridge
      graph_bridge::queue_point(1, g_graphs.m_gcurves[0].m_plot_id, g_demo.time, ke);
      graph_bridge::queue_point(1, g_graphs.m_gcurves[1].m_plot_id, g_demo.time, pe);
      graph_bridge::queue_point(1, g_graphs.m_gcurves[2].m_plot_id, g_demo.time, total);

      // Mark graph dirty for bounds update
      if (!g_graphs.m_graphs.empty()) {
        g_graphs.m_graphs[0].m_dirty = true;
      }
    }
    frame_counter++;

    g_demo.time += sim_dt;
    scene.mark_dirty();
  }
}

export int run_web_test() {
  using namespace vcpp;
  using namespace vcpp::wgpu::web;

  // Initialize WebGPU backend
  if (!init(scene, "#canvas")) {
    return 1;
  }
  
  // Load initial scene
  load_scene_default();

  // Start main loop
  run(update);

  return 0;
}
