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
  
  enum class SceneID { Default, Solar, Random, Rain, CoroRain, Torque, Graph, Curve, Points, Labels, Trails, TriQuad, Compound, Text3D, Extrusion };
  SceneID current_scene = SceneID::Default;
  
  // Forward declaration for coroutine rain scene
  bool is_coro_rain_scene = false;

  // Graph demo state
  struct graph_demo_state {
    double ball_y{5.0};
    double ball_vy{0.0};
    double time{0.0};
    std::size_t ball_idx{0};
    bool active{false};
  } g_demo;

  // Curve demo state
  struct curve_demo_state {
    std::size_t dynamic_curve_idx{0};
    double time{0.0};
    bool active{false};
  } c_demo;

  // Forward declarations for demos defined later
  struct points_demo_state {
    std::size_t points_idx{0};
    double time{0.0};
    bool active{false};
  };
  extern points_demo_state p_demo;

  struct trails_demo_state {
    std::vector<std::size_t> ball_indices;
    double time{0.0};
    bool active{false};
  };
  extern trails_demo_state tr_demo;

  struct compound_demo_state {
    std::size_t compound_idx{0};
    double time{0.0};
    bool active{false};
  };
  extern compound_demo_state cp_demo;

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

  void reset_all_demos() {
    is_raining_scene = false;
    is_coro_rain_scene = false;  // Stop coroutine rain
    t_sim.active = false;
    g_demo.active = false;
    c_demo.active = false;
    p_demo.active = false;
    tr_demo.active = false;
    cp_demo.active = false;
  }

  void load_scene_default() {
    current_scene = SceneID::Default;
    using namespace vcpp;
    scene.clear();
    graph_bridge::clear_all();  // Clear any graphs
    reset_all_demos();
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
    reset_all_demos();
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
    reset_all_demos();
    t_sim.active = true;
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
      c_demo.active = false;
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
    c_demo.active = false;
    
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
  // SCENE 4b: COROUTINE-BASED Raindrop Demo
  // Demonstrates task<void>, co_await next_frame(), and per-instance shader effects
  //
  // Shader effect protocol (via material vec4 when emissive=true):
  //   material.y = 1.0 (emissive flag → activates raindrop shader path)
  //   material.z = normalized speed (0=still, 1=terminal velocity)
  //   material.w = lifecycle phase (0=falling, 1=fully splatted)
  // ==========================================================================
  
  // Terminal velocity estimate for normalizing speed
  constexpr double terminal_velocity = 8.0;

  // Coroutine that manages a single raindrop lifecycle
  vcpp::task<void> raindrop_coro(std::size_t idx, double start_x, double start_z, double size)
  {
    using namespace vcpp;
    
    // Get reference to our ellipsoid
    if (idx >= scene.m_ellipsoids.size()) co_return;
    auto& obj = scene.m_ellipsoids[idx];
    
    // Activate raindrop shader effect
    obj.m_emissive = true;
    
    // Random reset position generator (seeded by index for variety)
    std::mt19937 rng(idx * 12345);
    std::uniform_real_distribution<> pos_dist(-7.0, 7.0);
    
    constexpr double ground_y = -dropheight;
    constexpr double floor_top_y = ground_y + 0.001;
    
    while (is_coro_rain_scene) // Loop forever while scene active
    {
      // === FALL PHASE ===
      vec3 velocity{0, 0, 0};
      obj.m_pos = vec3{start_x, dropheight, start_z};
      obj.m_width = size;
      obj.m_length = size;
      obj.m_height = size;
      obj.m_effect_param0 = 0.0; // speed = 0 at start
      obj.m_effect_param1 = 0.0; // phase = falling
      obj.m_opacity = 1.0;
      
      while (obj.m_pos.y() - obj.m_height * 0.5 > floor_top_y)
      {
        double dt = co_await next_frame();
        if (!is_coro_rain_scene) co_return;
        
        // Air drag (simplified)
        double v_abs = std::abs(velocity.y());
        double drag = 0.5 * v_abs * v_abs * 0.01;
        double sign_v = velocity.y() > 0 ? 1.0 : -1.0;
        
        velocity = vec3{0, velocity.y() + (gravity + sign_v * drag) * dt, 0};
        obj.m_pos = obj.m_pos + velocity * dt;
        
        // Update shader effect: normalized speed
        double norm_speed = std::min(std::abs(velocity.y()) / terminal_velocity, 1.0);
        obj.m_effect_param0 = norm_speed;
        
        // Air deformation
        double AR = 1.0 - 0.02 * std::abs(velocity.y());
        if (AR < 0.3) AR = 0.3;
        double vol = size * size * size;
        double w_new = std::cbrt(vol / AR);
        obj.m_width = w_new;
        obj.m_length = w_new;
        obj.m_height = w_new * AR;
        
        scene.mark_dirty();
      }
      
      // Snap to ground
      obj.m_pos = vec3{obj.m_pos.x(), floor_top_y + obj.m_height * 0.5, obj.m_pos.z()};
      
      // === SPLAT PHASE ===
      double vol = obj.m_width * obj.m_width * obj.m_height;
      double original_radius = std::cbrt(vol);
      double target_h = original_radius * 0.15;
      if (target_h < 0.01) target_h = 0.01;
      obj.m_effect_param0 = 0.0; // speed drops to 0 on impact
      
      for (int frame = 0; frame < 15; ++frame) // ~0.25 seconds at 60fps
      {
        double dt = co_await next_frame();
        if (!is_coro_rain_scene) co_return;
        
        // Animate flattening (fast exponential approach)
        double smooth = 6.0 * dt;
        if (smooth > 1.0) smooth = 1.0;
        
        double new_h = obj.m_height + (target_h - obj.m_height) * smooth;
        obj.m_height = new_h;
        double w_new = std::sqrt(vol / new_h);
        obj.m_width = w_new;
        obj.m_length = w_new;
        obj.m_pos = vec3{obj.m_pos.x(), floor_top_y + new_h * 0.5, obj.m_pos.z()};
        
        // Update shader effect: phase ramps 0 → 1 during splat
        double phase = static_cast<double>(frame) / 15.0;
        obj.m_effect_param1 = phase;
        
        scene.mark_dirty();
        
        if (std::abs(obj.m_height - target_h) < 0.001) {
          obj.m_effect_param1 = 1.0;
          break;
        }
      }
      
      // === PAUSE THEN RESET ===
      for (int i = 0; i < 20; ++i) // Wait ~0.33 seconds
      {
        co_await next_frame();
        if (!is_coro_rain_scene) co_return;
      }
      
      // Reset for next drop (randomize position)
      start_x = pos_dist(rng);
      start_z = pos_dist(rng);
    }
  }
  
  // Storage for active raindrop coroutines
  std::vector<vcpp::task<void>> rain_tasks;
  
  // Number of active rain coroutines
  int coro_rain_active_count = 0;
  constexpr int coro_rain_max = 80;
  // Index of next pre-allocated ellipsoid to activate
  int coro_rain_next_slot = 0;
  // Offset of first rain ellipsoid in scene.m_ellipsoids
  std::size_t coro_rain_base_idx = 0;
  
  // Spawner coroutine: activates pre-allocated raindrops at random intervals
  vcpp::task<void> rain_spawner_coro()
  {
    using namespace vcpp;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> size_dist(0.15, 0.4);
    std::uniform_real_distribution<> pos_dist(-7.0, 7.0);
    std::uniform_real_distribution<> start_y_dist(0.0, dropheight * 1.5);
    std::uniform_int_distribution<> delay_dist(6, 30); // 0.1s to 0.5s at 60fps
    
    while (is_coro_rain_scene)
    {
      if (coro_rain_next_slot < coro_rain_max)
      {
        std::size_t idx = coro_rain_base_idx + coro_rain_next_slot;
        double size = size_dist(gen);
        double x = pos_dist(gen);
        double z = pos_dist(gen);
        
        // Activate the pre-allocated ellipsoid
        auto& obj = scene.m_ellipsoids[idx];
        obj.m_visible = true;
        obj.m_pos = vec3{x, start_y_dist(gen), z}; // Randomize start height
        obj.m_width = size;
        obj.m_length = size;
        obj.m_height = size;
        
        coro_rain_next_slot++;
        coro_rain_active_count++;
        
        // Start the lifecycle coroutine
        auto task = raindrop_coro(idx, x, z, size);
        task.detach();
      }
      
      // Wait a random interval before activating next drop
      int delay_frames = delay_dist(gen);
      for (int i = 0; i < delay_frames; ++i)
      {
        co_await next_frame();
        if (!is_coro_rain_scene) co_return;
      }
    }
  }

  void load_scene_coro_rain()
  {
    current_scene = SceneID::CoroRain;
    using namespace vcpp;
    scene.clear();
    graph_bridge::clear_all();
    reset_all_demos();
    
    is_coro_rain_scene = true;
    rain_tasks.clear();
    coro_rain_active_count = 0;
    coro_rain_next_slot = 0;
    
    scene.background(vec3{0.65, 0.72, 0.82}); // Overcast sky
    
    double worldsize = 20.0;
    constexpr int initial_batch = 15;
    constexpr double spawn_area = 14.0;
    double allowed = spawn_area / 2.0;
    
    // Floor
    scene.add(box(length=worldsize, height=0.001, width=worldsize, 
                  pos=vec3{0, -dropheight, 0}, color=vec3{0.18, 0.2, 0.18})); // Dark wet ground
    
    // Random generators
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> size_dist(0.15, 0.4);
    std::uniform_real_distribution<> pos_dist(-allowed, allowed);
    std::uniform_real_distribution<> start_y_dist(0.0, dropheight * 1.5);
    
    // Pre-allocate ALL ellipsoids upfront (hidden) to prevent vector reallocation
    // while coroutines hold references into the vector
    coro_rain_base_idx = scene.m_ellipsoids.size();
    for (int i = 0; i < coro_rain_max; ++i)
    {
      scene.add(ellipsoid(length=0.5, height=0.5, width=0.5,
                          pos=vec3{0, dropheight, 0},
                          color=vec3{0.5, 0.7, 1.0},
                          emissive=true, visible=false));
    }
    
    // Activate initial batch immediately for instant visual feedback
    for (int i = 0; i < initial_batch; ++i)
    {
      std::size_t idx = coro_rain_base_idx + i;
      double size = size_dist(gen);
      double x = pos_dist(gen);
      double z = pos_dist(gen);
      
      auto& obj = scene.m_ellipsoids[idx];
      obj.m_visible = true;
      obj.m_pos = vec3{x, start_y_dist(gen), z}; // Randomize start height
      obj.m_width = size;
      obj.m_length = size;
      obj.m_height = size;
      
      coro_rain_next_slot++;
      coro_rain_active_count++;
      
      auto task = raindrop_coro(idx, x, z, size);
      task.detach();
    }
    
    // Start the continuous spawner coroutine (activates remaining pre-allocated drops)
    auto spawner = rain_spawner_coro();
    spawner.detach();
    
    std::cout << "Loaded COROUTINE Rain Scene (Press `)" << std::endl;
    std::cout << "  " << initial_batch << " initial drops + continuous spawning (max " << coro_rain_max << ")" << std::endl;
    std::cout << "  Shader: Fresnel rim, velocity color, splat fade" << std::endl;
    std::cout << "  Each drop: fall -> splat -> pause -> reset" << std::endl;
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
    c_demo.active = false;

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
    auto ke_curve = gcurve(color=colors::red, prop::label="KE", width=2.0);
    auto pe_curve = gcurve(color=colors::blue, prop::label="PE", width=2.0);
    auto te_curve = gcurve(color=colors::green, prop::label="Total", width=2.0);

    g_graphs.add(ke_curve, g_graphs.m_graphs[0]);
    g_graphs.add(pe_curve, g_graphs.m_graphs[0]);
    g_graphs.add(te_curve, g_graphs.m_graphs[0]);

    // Sync graphs to JavaScript
    graph_bridge::sync_all();

    std::cout << "Loaded Graph Demo Scene (Press 6)" << std::endl;
    std::cout << "  Red ball bounces with energy loss" << std::endl;
    std::cout << "  Graph shows: KE (red), PE (blue), Total (green)" << std::endl;
  }

  // ==========================================================================
  // SCENE 7: Curve Object Demo
  // ==========================================================================
  void load_scene_curve() {
    current_scene = SceneID::Curve;
    using namespace vcpp;
    scene.clear();
    graph_bridge::clear_all();
    is_raining_scene = false;
    t_sim.active = false;
    g_demo.active = false;
    c_demo.active = true;
    c_demo.time = 0.0;

    scene.background(vec3{0.05, 0.05, 0.1});

    // Static spiral curve (like a spring/helix but using curve)
    auto spiral = curve(color=colors::cyan, radius=0.08);
    int spiral_points = 100;
    double spiral_radius = 2.0;
    double spiral_height = 4.0;
    double spiral_coils = 5.0;

    for (int i = 0; i < spiral_points; ++i) {
      double t = static_cast<double>(i) / (spiral_points - 1);
      double angle = t * spiral_coils * 2.0 * 3.14159265;
      double x = spiral_radius * std::cos(angle);
      double y = t * spiral_height - spiral_height / 2.0;
      double z = spiral_radius * std::sin(angle);
      spiral.append(vec3{x, y, z});
    }
    scene.add(spiral);

    // Dynamic curve that grows over time (starts empty)
    auto growing = curve(color=colors::yellow, radius=0.05);
    scene.add(growing);
    c_demo.dynamic_curve_idx = scene.m_curves.size() - 1;

    // A second static curve: figure-8 / lemniscate
    auto figure8 = curve(color=colors::magenta, radius=0.06);
    int f8_points = 80;
    double f8_scale = 1.5;
    for (int i = 0; i < f8_points; ++i) {
      double t = static_cast<double>(i) / (f8_points - 1) * 2.0 * 3.14159265;
      double denom = 1.0 + std::sin(t) * std::sin(t);
      double x = f8_scale * std::cos(t) / denom + 4.0; // offset to the right
      double y = 0.0;
      double z = f8_scale * std::sin(t) * std::cos(t) / denom;
      figure8.append(vec3{x, y, z});
    }
    scene.add(figure8);

    std::cout << "Loaded Curve Demo Scene (Press 7)" << std::endl;
    std::cout << "  Cyan: Static spiral curve" << std::endl;
    std::cout << "  Yellow: Dynamic growing curve" << std::endl;
    std::cout << "  Magenta: Figure-8 (lemniscate)" << std::endl;
  }

  // ==========================================================================
  // SCENE 8: Points Demo (NEW)
  // ==========================================================================
  points_demo_state p_demo;

  void load_scene_points() {
    current_scene = SceneID::Points;
    using namespace vcpp;
    scene.clear();
    graph_bridge::clear_all();
    is_raining_scene = false;
    t_sim.active = false;
    g_demo.active = false;
    c_demo.active = false;
    p_demo.active = true;
    p_demo.time = 0.0;

    scene.background(vec3{0.02, 0.02, 0.05});

    // Create a points object for particle visualization
    auto pts = points(color=colors::cyan, size=8.0);

    // Initialize with random points in a sphere
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dist(-1.0, 1.0);

    for (int i = 0; i < 500; ++i) {
      vec3 p;
      do {
        p = vec3{dist(gen), dist(gen), dist(gen)};
      } while (mag(p) > 1.0); // Reject points outside unit sphere
      p = p * 3.0; // Scale up
      pts.append(p);
    }

    scene.add(pts);
    p_demo.points_idx = scene.m_points.size() - 1;

    // Add a central reference sphere
    scene.add(sphere(pos=vec3{0,0,0}, radius=0.3, color=colors::yellow, opacity=0.5));

    std::cout << "Loaded Points Demo Scene (Press 8)" << std::endl;
    std::cout << "  500 animated particles in a sphere" << std::endl;
    std::cout << "  Points pulse outward over time" << std::endl;
  }

  // ==========================================================================
  // SCENE 9: Labels Demo (NEW)
  // ==========================================================================
  void load_scene_labels() {
    current_scene = SceneID::Labels;
    using namespace vcpp;
    scene.clear();
    graph_bridge::clear_all();
    is_raining_scene = false;
    t_sim.active = false;
    g_demo.active = false;
    c_demo.active = false;
    p_demo.active = false;

    scene.background(vec3{0.1, 0.1, 0.15});

    // Create some labeled objects
    scene.add(sphere(pos=vec3{-3, 0, 0}, radius=0.8, color=colors::red));
    auto lbl1 = label(pos=vec3{-3, 1.5, 0}, color=colors::white, height=20.0, prop::box=true)
                  .text("Red Sphere");
    scene.add(lbl1);

    scene.add(box(pos=vec3{0, 0, 0}, length=1.5, height=1.5, width=1.5, color=colors::green));
    auto lbl2 = label(pos=vec3{0, 1.5, 0}, color=colors::yellow, height=18.0, prop::box=true)
                  .text("Green Box");
    scene.add(lbl2);

    scene.add(cylinder(pos=vec3{3, -0.5, 0}, axis=vec3{0, 1, 0}, radius=0.5, length=1.0, color=colors::blue));
    auto lbl3 = label(pos=vec3{3, 1.2, 0}, color=colors::cyan, height=16.0, xoffset=30.0)
                  .text("Blue Cylinder");
    scene.add(lbl3);

    // Info label at top
    auto info = label(pos=vec3{0, 3, 0}, color=colors::white, height=24.0)
                  .text("Labels Demo - 2D text overlay at 3D positions");
    scene.add(info);

    std::cout << "Loaded Labels Demo Scene (Press 9)" << std::endl;
    std::cout << "  3 objects with 2D text labels" << std::endl;
  }

  // ==========================================================================
  // SCENE 10: Trails Demo (NEW)
  // ==========================================================================
  trails_demo_state tr_demo;

  void load_scene_trails() {
    current_scene = SceneID::Trails;
    using namespace vcpp;
    scene.clear();
    graph_bridge::clear_all();
    is_raining_scene = false;
    t_sim.active = false;
    g_demo.active = false;
    c_demo.active = false;
    p_demo.active = false;
    tr_demo.active = true;
    tr_demo.time = 0.0;
    tr_demo.ball_indices.clear();

    scene.background(vec3{0.05, 0.0, 0.1});

    // Create spheres with trails enabled
    auto ball1 = sphere(pos=vec3{2, 0, 0}, radius=0.3, color=colors::red,
                        make_trail=true, trail_color=colors::orange, retain=3.0);
    scene.add(ball1);
    tr_demo.ball_indices.push_back(scene.m_spheres.size() - 1);

    auto ball2 = sphere(pos=vec3{0, 2, 0}, radius=0.3, color=colors::green,
                        make_trail=true, trail_color=colors::cyan, retain=3.0);
    scene.add(ball2);
    tr_demo.ball_indices.push_back(scene.m_spheres.size() - 1);

    auto ball3 = sphere(pos=vec3{0, 0, 2}, radius=0.3, color=colors::blue,
                        make_trail=true, trail_color=colors::magenta, retain=3.0);
    scene.add(ball3);
    tr_demo.ball_indices.push_back(scene.m_spheres.size() - 1);

    // Central reference point
    scene.add(sphere(pos=vec3{0,0,0}, radius=0.1, color=colors::white));

    std::cout << "Loaded Trails Demo Scene (Press 0)" << std::endl;
    std::cout << "  3 orbiting spheres leaving trails" << std::endl;
  }

  // ==========================================================================
  // SCENE 11: Triangle/Quad Demo (NEW)
  // ==========================================================================
  void load_scene_triquad() {
    current_scene = SceneID::TriQuad;
    using namespace vcpp;
    scene.clear();
    graph_bridge::clear_all();
    is_raining_scene = false;
    t_sim.active = false;
    g_demo.active = false;
    c_demo.active = false;
    p_demo.active = false;
    tr_demo.active = false;

    scene.background(vec3{0.1, 0.1, 0.1});

    // Create custom triangles
    triangle_object tri1;
    tri1.m_v0 = {vec3{-3, 0, 0}, vec3{0, 0, 1}, colors::red, 1.0, vec2{0, 0}};
    tri1.m_v1 = {vec3{-2, 0, 0}, vec3{0, 0, 1}, colors::green, 1.0, vec2{1, 0}};
    tri1.m_v2 = {vec3{-2.5, 1.5, 0}, vec3{0, 0, 1}, colors::blue, 1.0, vec2{0.5, 1}};
    scene.add(tri1);

    // Create custom quad
    quad_object q1;
    q1.m_v0 = {vec3{0, 0, 0}, vec3{0, 0, 1}, colors::yellow, 1.0, vec2{0, 0}};
    q1.m_v1 = {vec3{2, 0, 0}, vec3{0, 0, 1}, colors::cyan, 1.0, vec2{1, 0}};
    q1.m_v2 = {vec3{2, 1.5, 0}, vec3{0, 0, 1}, colors::magenta, 1.0, vec2{1, 1}};
    q1.m_v3 = {vec3{0, 1.5, 0}, vec3{0, 0, 1}, colors::white, 1.0, vec2{0, 1}};
    scene.add(q1);

    // Create a pyramid-like shape with triangles
    vec3 apex{4, 2, 0};
    vec3 base1{3, 0, -1};
    vec3 base2{5, 0, -1};
    vec3 base3{5, 0, 1};
    vec3 base4{3, 0, 1};

    // Four triangular faces
    triangle_object f1;
    f1.m_v0 = {apex, hat(apex - (base1 + base2) * 0.5), colors::red, 1.0, vec2{0.5, 1}};
    f1.m_v1 = {base1, hat(apex - (base1 + base2) * 0.5), colors::red, 1.0, vec2{0, 0}};
    f1.m_v2 = {base2, hat(apex - (base1 + base2) * 0.5), colors::red, 1.0, vec2{1, 0}};
    scene.add(f1);

    triangle_object f2;
    f2.m_v0 = {apex, hat(apex - (base2 + base3) * 0.5), colors::green, 1.0, vec2{0.5, 1}};
    f2.m_v1 = {base2, hat(apex - (base2 + base3) * 0.5), colors::green, 1.0, vec2{0, 0}};
    f2.m_v2 = {base3, hat(apex - (base2 + base3) * 0.5), colors::green, 1.0, vec2{1, 0}};
    scene.add(f2);

    triangle_object f3;
    f3.m_v0 = {apex, hat(apex - (base3 + base4) * 0.5), colors::blue, 1.0, vec2{0.5, 1}};
    f3.m_v1 = {base3, hat(apex - (base3 + base4) * 0.5), colors::blue, 1.0, vec2{0, 0}};
    f3.m_v2 = {base4, hat(apex - (base3 + base4) * 0.5), colors::blue, 1.0, vec2{1, 0}};
    scene.add(f3);

    triangle_object f4;
    f4.m_v0 = {apex, hat(apex - (base4 + base1) * 0.5), colors::yellow, 1.0, vec2{0.5, 1}};
    f4.m_v1 = {base4, hat(apex - (base4 + base1) * 0.5), colors::yellow, 1.0, vec2{0, 0}};
    f4.m_v2 = {base1, hat(apex - (base4 + base1) * 0.5), colors::yellow, 1.0, vec2{1, 0}};
    scene.add(f4);

    std::cout << "Loaded Triangle/Quad Demo Scene (Press -)" << std::endl;
    std::cout << "  RGB gradient triangle, color gradient quad, and custom pyramid" << std::endl;
  }

  // ==========================================================================
  // SCENE 12: Compound Demo (NEW)
  // ==========================================================================
  compound_demo_state cp_demo;

  void load_scene_compound() {
    current_scene = SceneID::Compound;
    using namespace vcpp;
    scene.clear();
    graph_bridge::clear_all();
    is_raining_scene = false;
    t_sim.active = false;
    g_demo.active = false;
    c_demo.active = false;
    p_demo.active = false;
    tr_demo.active = false;
    cp_demo.active = true;
    cp_demo.time = 0.0;

    scene.background(vec3{0.1, 0.1, 0.15});

    // Create a compound object (robot-like figure)
    auto robot = compound(
      // Head
      sphere(pos=vec3{0, 2, 0}, radius=0.5, color=colors::cyan),
      // Body
      box(pos=vec3{0, 0.8, 0}, length=0.8, height=1.5, width=0.5, color=colors::blue),
      // Left arm
      cylinder(pos=vec3{-0.7, 1.2, 0}, axis=vec3{-0.8, -0.5, 0}, radius=0.15, length=1.0, color=colors::green),
      // Right arm
      cylinder(pos=vec3{0.7, 1.2, 0}, axis=vec3{0.8, -0.5, 0}, radius=0.15, length=1.0, color=colors::green),
      // Left leg
      cylinder(pos=vec3{-0.25, -0.2, 0}, axis=vec3{0, -1.2, 0}, radius=0.15, length=1.2, color=colors::yellow),
      // Right leg
      cylinder(pos=vec3{0.25, -0.2, 0}, axis=vec3{0, -1.2, 0}, radius=0.15, length=1.2, color=colors::yellow)
    );

    scene.add(robot);
    cp_demo.compound_idx = scene.m_compounds.size() - 1;

    // Add some regular objects for comparison
    scene.add(sphere(pos=vec3{3, 0, 0}, radius=0.5, color=colors::red));
    scene.add(box(pos=vec3{-3, 0, 0}, length=1.0, height=1.0, width=1.0, color=colors::magenta));

    std::cout << "Loaded Compound Demo Scene (Press =)" << std::endl;
    std::cout << "  Robot figure made from compound of 6 objects" << std::endl;
    std::cout << "  Compound rotates as a single unit" << std::endl;
  }

  // ==========================================================================
  // SCENE 13: 3D Text Demo (NEW)
  // ==========================================================================
  void load_scene_text3d() {
    current_scene = SceneID::Text3D;
    using namespace vcpp;
    scene.clear();
    graph_bridge::clear_all();
    is_raining_scene = false;
    t_sim.active = false;
    g_demo.active = false;
    c_demo.active = false;
    p_demo.active = false;
    tr_demo.active = false;
    cp_demo.active = false;

    scene.background(vec3{0.05, 0.05, 0.1});

    // Create 3D text objects
    auto txt1 = text3d(pos=vec3{-4, 2, 0}, height=1.0, color=colors::red, axis=vec3{1, 0, 0})
                  .text("HELLO");
    scene.add(txt1);

    auto txt2 = text3d(pos=vec3{-2, 0, 0}, height=0.8, color=colors::green, axis=vec3{1, 0, 0})
                  .text("VCPP");
    scene.add(txt2);

    auto txt3 = text3d(pos=vec3{0, -2, 0}, height=0.6, color=colors::cyan, axis=vec3{1, 0.2, 0})
                  .text("3D TEXT!");
    scene.add(txt3);

    // Numbers
    auto nums = text3d(pos=vec3{2, 1, 0}, height=0.5, color=colors::yellow)
                  .text("0123456789");
    scene.add(nums);

    std::cout << "Loaded 3D Text Demo Scene (Press [)" << std::endl;
    std::cout << "  Extruded 3D text with different sizes and colors" << std::endl;
  }

  // ==========================================================================
  // SCENE 14: Extrusion Demo (NEW)
  // ==========================================================================
  void load_scene_extrusion() {
    current_scene = SceneID::Extrusion;
    using namespace vcpp;
    scene.clear();
    graph_bridge::clear_all();
    is_raining_scene = false;
    t_sim.active = false;
    g_demo.active = false;
    c_demo.active = false;
    p_demo.active = false;
    tr_demo.active = false;
    cp_demo.active = false;

    scene.background(vec3{0.08, 0.08, 0.12});

    // Circular extrusion along spiral path
    extrusion_object ext1;
    ext1.m_color = colors::cyan;
    ext1.set_shape(shapes::circle(0.3, 16).points());

    // Spiral path
    for (int i = 0; i < 50; ++i) {
      double t = static_cast<double>(i) / 49.0;
      double angle = t * 4.0 * 3.14159265;
      double r = 1.0 + t * 1.5;
      double x = r * std::cos(angle) - 4.0;
      double y = t * 3.0 - 1.5;
      double z = r * std::sin(angle);
      ext1.append_path(vec3{x, y, z});
    }
    scene.add(ext1);

    // Star-shaped extrusion along straight path
    extrusion_object ext2;
    ext2.m_color = colors::yellow;
    ext2.m_twist = 3.14159265; // 180 degree twist
    ext2.set_shape(shapes::star(5, 0.4, 0.2).points());

    for (int i = 0; i < 20; ++i) {
      double t = static_cast<double>(i) / 19.0;
      ext2.append_path(vec3{0, t * 4.0 - 2.0, 0});
    }
    scene.add(ext2);

    // Rectangular extrusion along curved path
    extrusion_object ext3;
    ext3.m_color = colors::magenta;
    ext3.m_scale = 0.5; // Taper to half size
    ext3.set_shape(shapes::rectangle(0.5, 0.3).points());

    for (int i = 0; i < 30; ++i) {
      double t = static_cast<double>(i) / 29.0;
      double x = 3.0 + std::sin(t * 3.14159265) * 2.0;
      double y = t * 3.0 - 1.5;
      double z = std::cos(t * 3.14159265) - 1.0;
      ext3.append_path(vec3{x, y, z});
    }
    scene.add(ext3);

    // Hexagon extrusion (pipe/tube)
    extrusion_object ext4;
    ext4.m_color = colors::green;
    ext4.set_shape(shapes::hexagon(0.25).points());

    // Sine wave path
    for (int i = 0; i < 40; ++i) {
      double t = static_cast<double>(i) / 39.0;
      double x = t * 6.0 - 3.0;
      double y = -2.5;
      double z = std::sin(t * 4.0 * 3.14159265) * 0.5 + 2.0;
      ext4.append_path(vec3{x, y, z});
    }
    scene.add(ext4);

    std::cout << "Loaded Extrusion Demo Scene (Press ])" << std::endl;
    std::cout << "  Cyan: Circular cross-section along spiral" << std::endl;
    std::cout << "  Yellow: Star shape with 180 deg twist" << std::endl;
    std::cout << "  Magenta: Rectangle tapering to 50%" << std::endl;
    std::cout << "  Green: Hexagon along sine wave" << std::endl;
  }
}

void update() {
  using namespace vcpp;
  t += 0.016;
  
  // === COROUTINE SCHEDULER TICK ===
  // Resume all pending coroutines each frame
  tick_coroutines(0.016);

  // Scene Switching
  if (g_input.consume_key("Digit1")) { std::cout << "Key 1 -> Default" << std::endl; load_scene_default(); }
  if (g_input.consume_key("Digit2")) { std::cout << "Key 2 -> Solar" << std::endl; load_scene_solar(); }
  if (g_input.consume_key("Digit3")) { std::cout << "Key 3 -> Random" << std::endl; add_random_objects(); }
  if (g_input.consume_key("Digit4")) { std::cout << "Key 4 -> Rain" << std::endl; load_scene_rain(); }
  if (g_input.consume_key("Backquote")) { std::cout << "Key ` -> Coro Rain" << std::endl; load_scene_coro_rain(); }
  if (g_input.consume_key("Digit5")) { std::cout << "Key 5 -> Torque" << std::endl; load_scene_torque(); }
  if (g_input.consume_key("Digit6")) { std::cout << "Key 6 -> Graph Demo" << std::endl; load_scene_graph(); }
  if (g_input.consume_key("Digit7")) { std::cout << "Key 7 -> Curve Demo" << std::endl; load_scene_curve(); }
  if (g_input.consume_key("Digit8")) { std::cout << "Key 8 -> Points Demo" << std::endl; load_scene_points(); }
  if (g_input.consume_key("Digit9")) { std::cout << "Key 9 -> Labels Demo" << std::endl; load_scene_labels(); }
  if (g_input.consume_key("Digit0")) { std::cout << "Key 0 -> Trails Demo" << std::endl; load_scene_trails(); }
  if (g_input.consume_key("Minus")) { std::cout << "Key - -> Triangle/Quad Demo" << std::endl; load_scene_triquad(); }
  if (g_input.consume_key("Equal")) { std::cout << "Key = -> Compound Demo" << std::endl; load_scene_compound(); }
  if (g_input.consume_key("BracketLeft")) { std::cout << "Key [ -> 3D Text Demo" << std::endl; load_scene_text3d(); }
  if (g_input.consume_key("BracketRight")) { std::cout << "Key ] -> Extrusion Demo" << std::endl; load_scene_extrusion(); }

  if (g_input.consume_key("KeyR")) {
    std::cout << "Resetting Scene..." << std::endl;
    switch(current_scene) {
      case SceneID::Default: load_scene_default(); break;
      case SceneID::Solar: load_scene_solar(); break;
      case SceneID::Random: add_random_objects(); break;
      case SceneID::Rain: load_scene_rain(); break;
      case SceneID::CoroRain: load_scene_coro_rain(); break;
      case SceneID::Torque: load_scene_torque(); break;
      case SceneID::Graph: load_scene_graph(); break;
      case SceneID::Curve: load_scene_curve(); break;
      case SceneID::Points: load_scene_points(); break;
      case SceneID::Labels: load_scene_labels(); break;
      case SceneID::Trails: load_scene_trails(); break;
      case SceneID::TriQuad: load_scene_triquad(); break;
      case SceneID::Compound: load_scene_compound(); break;
      case SceneID::Text3D: load_scene_text3d(); break;
      case SceneID::Extrusion: load_scene_extrusion(); break;
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
      std::size_t graph_id = g_graphs.m_graphs[0].m_id;
      graph_bridge::queue_point(graph_id, g_graphs.m_gcurves[0].m_plot_id, g_demo.time, ke);
      graph_bridge::queue_point(graph_id, g_graphs.m_gcurves[1].m_plot_id, g_demo.time, pe);
      graph_bridge::queue_point(graph_id, g_graphs.m_gcurves[2].m_plot_id, g_demo.time, total);

      // Mark graph dirty for bounds update
      if (!g_graphs.m_graphs.empty()) {
        g_graphs.m_graphs[0].m_dirty = true;
      }
    }
    frame_counter++;

    g_demo.time += sim_dt;
    scene.mark_dirty();
  }
  else if (c_demo.active) {
    // Animate the growing curve
    c_demo.time += sim_dt;

    if (c_demo.dynamic_curve_idx < scene.m_curves.size()) {
      auto& crv = scene.m_curves[c_demo.dynamic_curve_idx];

      // Add a new point every few frames (spiral pattern growing outward)
      static int frame_count = 0;
      if (frame_count % 5 == 0) {
        double t = c_demo.time;
        double wave_radius = 0.5 + t * 0.3;
        double x = -4.0 + wave_radius * std::cos(t * 3.0);
        double y = wave_radius * std::sin(t * 5.0);
        double z = wave_radius * std::sin(t * 3.0);

        crv.append(vec3{x, y, z});

        // Limit points to prevent unbounded growth
        if (crv.m_points.size() > 500) {
          crv.m_points.erase(crv.m_points.begin());
          crv.m_geometry_dirty = true;
        }
      }
      frame_count++;
    }
    scene.mark_dirty();
  }
  else if (p_demo.active) {
    // Animate points - pulsing outward
    p_demo.time += sim_dt;

    if (p_demo.points_idx < scene.m_points.size()) {
      auto& pts = scene.m_points[p_demo.points_idx];

      // Pulse the points outward and back
      double pulse = 1.0 + 0.3 * std::sin(p_demo.time * 2.0);

      for (std::size_t i = 0; i < pts.m_points.size(); ++i) {
        vec3& p = pts.m_points[i];
        double r = mag(p);
        if (r > 0.001) {
          // Add some rotation too
          double angle = p_demo.time * 0.5;
          double x = p.x() * std::cos(angle * 0.1) - p.z() * std::sin(angle * 0.1);
          double z = p.x() * std::sin(angle * 0.1) + p.z() * std::cos(angle * 0.1);
          p = vec3{x, p.y(), z};
        }
      }
      pts.m_geometry_dirty = true;

      // Also animate the color
      double hue = std::fmod(p_demo.time * 0.2, 1.0);
      pts.m_color = vec3{
        0.5 + 0.5 * std::sin(hue * 6.28),
        0.5 + 0.5 * std::sin(hue * 6.28 + 2.09),
        0.5 + 0.5 * std::sin(hue * 6.28 + 4.19)
      };
    }
    scene.mark_dirty();
  }
  else if (tr_demo.active) {
    // Animate trailing spheres in orbits
    tr_demo.time += sim_dt;

    for (std::size_t i = 0; i < tr_demo.ball_indices.size(); ++i) {
      if (tr_demo.ball_indices[i] < scene.m_spheres.size()) {
        auto& ball = scene.m_spheres[tr_demo.ball_indices[i]];

        double t = tr_demo.time;
        double phase = static_cast<double>(i) * 2.09; // 120 degrees apart
        double orbit_speed = 1.0 + i * 0.3;

        // Different orbit planes for each ball
        double x, y, z;
        if (i == 0) {
          // XY plane orbit
          x = 2.0 * std::cos(t * orbit_speed);
          y = 2.0 * std::sin(t * orbit_speed);
          z = 0.0;
        } else if (i == 1) {
          // XZ plane orbit
          x = 2.0 * std::cos(t * orbit_speed + phase);
          y = 0.0;
          z = 2.0 * std::sin(t * orbit_speed + phase);
        } else {
          // YZ plane orbit
          x = 0.0;
          y = 2.0 * std::cos(t * orbit_speed + phase);
          z = 2.0 * std::sin(t * orbit_speed + phase);
        }

        ball.m_pos = vec3{x, y, z};
      }
    }
    scene.mark_dirty();
  }
  else if (cp_demo.active) {
    // Rotate the compound object
    cp_demo.time += sim_dt;

    if (cp_demo.compound_idx < scene.m_compounds.size()) {
      auto& comp = scene.m_compounds[cp_demo.compound_idx];

      // Rotate around Y axis
      double angle = cp_demo.time * 0.5;
      comp.m_axis = vec3{std::sin(angle), 0, std::cos(angle)};

      // Also bob up and down
      comp.m_pos = vec3{0, std::sin(cp_demo.time * 2.0) * 0.5, 0};
    }
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
