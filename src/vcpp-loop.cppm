/*
 *  vcpp:loop - Animation loop and timing
 *
 *  Provides:
 *  - rate(): VPython-style frame rate limiter
 *  - loop(): main animation loop
 *  - frame_timer: precise timing control
 */

module;

import std;

export module vcpp:loop;

import :scene;

export namespace vcpp
{

// ============================================================================
// Frame Timer - Precise timing for animation
// ============================================================================

class frame_timer
{
public:
  using clock = std::chrono::steady_clock;
  using duration = std::chrono::duration<double>;
  using time_point = clock::time_point;

private:
  time_point m_last_frame{clock::now()};
  double m_target_dt{1.0 / 60.0}; // target frame time (seconds)
  double m_actual_dt{0.0};        // actual elapsed time
  std::size_t m_frame_count{0};

public:
  // Set target frame rate
  void set_rate(double fps) noexcept
  {
    if (fps > 0)
      m_target_dt = 1.0 / fps;
  }

  double target_fps() const noexcept { return 1.0 / m_target_dt; }

  // Wait until enough time has passed, return actual dt
  double wait() noexcept
  {
    auto now = clock::now();
    m_actual_dt = duration(now - m_last_frame).count();

    // Sleep if we're ahead of schedule
    if (m_actual_dt < m_target_dt)
    {
      double sleep_time = m_target_dt - m_actual_dt;

      // Sleep for most of the remaining time (leave some for spin-wait)
      if (sleep_time > 0.002) // only sleep if > 2ms remaining
      {
        std::this_thread::sleep_for(std::chrono::duration<double>(sleep_time * 0.9));
      }

      // Spin-wait for precise timing
      while (duration(clock::now() - m_last_frame).count() < m_target_dt)
      {
        // busy wait
      }

      now = clock::now();
      m_actual_dt = duration(now - m_last_frame).count();
    }

    m_last_frame = now;
    m_frame_count++;
    return m_actual_dt;
  }

  // Get timing info
  double actual_dt() const noexcept { return m_actual_dt; }
  double actual_fps() const noexcept { return m_actual_dt > 0 ? 1.0 / m_actual_dt : 0; }
  std::size_t frame_count() const noexcept { return m_frame_count; }

  // Reset timer
  void reset() noexcept
  {
    m_last_frame = clock::now();
    m_frame_count = 0;
  }
};

// ============================================================================
// Global Timer
// ============================================================================

inline frame_timer global_timer{};

// ============================================================================
// rate() - VPython-style frame rate limiter
//
// Call once per iteration in your animation loop.
// Returns the actual time elapsed since last call.
//
// Usage:
//   while (running) {
//     rate(60);  // max 60 fps
//     // update physics, etc.
//   }
// ============================================================================

inline double rate(double fps) noexcept
{
  global_timer.set_rate(fps);
  return global_timer.wait();
}

// ============================================================================
// Render Callbacks (to be implemented by backend)
//
// These are forward-declared here and defined by the rendering backend.
// ============================================================================

// Check if window should close
inline bool (*window_should_close_fn)() = nullptr;

// Poll window events
inline void (*poll_events_fn)() = nullptr;

// Render a frame
inline void (*render_frame_fn)(canvas&) = nullptr;

// Default implementations (no-op until backend is connected)
inline bool window_should_close() noexcept { return window_should_close_fn ? window_should_close_fn() : false; }

inline void poll_events() noexcept
{
  if (poll_events_fn)
    poll_events_fn();
}

inline void render_frame(canvas& c)
{
  if (render_frame_fn)
    render_frame_fn(c);
}

// ============================================================================
// loop() - Main animation loop
//
// Runs the animation loop with the given update function.
// Handles event polling, rendering, and frame timing.
//
// Usage:
//   loop([&] {
//     ball.m_pos = ball.m_pos + velocity * dt;
//     scene.mark_dirty();
//   }, 60);  // 60 fps
// ============================================================================

template<typename UpdateFunc>
void loop(UpdateFunc&& update, double fps = 60.0, canvas& c = scene)
{
  global_timer.set_rate(fps);
  global_timer.reset();

  while (!window_should_close())
  {
    // Poll window events
    poll_events();

    // User update function
    update();

    // Render if scene changed
    if (c.is_dirty())
    {
      render_frame(c);
      c.clear_dirty();
    }

    // Frame timing
    global_timer.wait();
  }
}

// ============================================================================
// run_once() - Single frame update (for manual control)
//
// Useful when you want more control over the loop.
//
// Usage:
//   while (!should_quit) {
//     double dt = run_once(60, scene);
//     // custom logic...
//   }
// ============================================================================

inline double run_once(double fps, canvas& c = scene)
{
  global_timer.set_rate(fps);

  poll_events();

  if (c.is_dirty())
  {
    render_frame(c);
    c.clear_dirty();
  }

  return global_timer.wait();
}

// ============================================================================
// Animation Helper Class
//
// Alternative to loop() for more complex scenarios.
// ============================================================================

class animation
{
public:
  animation(double fps = 60.0, canvas& c = scene) : m_canvas{c}, m_running{true}
  {
    m_timer.set_rate(fps);
    m_timer.reset();
  }

  bool running() const noexcept { return m_running && !window_should_close(); }

  void stop() noexcept { m_running = false; }

  // Call each frame, returns dt
  double tick()
  {
    poll_events();

    if (m_canvas.is_dirty())
    {
      render_frame(m_canvas);
      m_canvas.clear_dirty();
    }

    return m_timer.wait();
  }

  // Convenience: use as functor
  double operator()() { return tick(); }

  // Access timing info
  double fps() const noexcept { return m_timer.actual_fps(); }
  std::size_t frame_count() const noexcept { return m_timer.frame_count(); }

private:
  canvas& m_canvas;
  frame_timer m_timer;
  bool m_running;
};

} // namespace vcpp
