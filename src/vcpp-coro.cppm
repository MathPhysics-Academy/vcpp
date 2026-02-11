/*
 *  vcpp:coro - C++20 Coroutine primitives for animation
 *
 *  Provides:
 *  - generator<T>: Lazy sequence generator (range-for compatible)
 *  - frames(fps): Generator yielding dt each frame
 *  - animate(duration, fps): Time-bounded animation generator
 */

module;

import std;

export module vcpp:coro;

export namespace vcpp
{

// ============================================================================
// generator<T> - Lazy sequence generator
//
// A symmetric-transfer generator for efficient iteration.
// Compatible with range-based for loops.
//
// Usage:
//   generator<int> count_to(int n) {
//     for (int i = 0; i < n; ++i) co_yield i;
//   }
//   for (int x : count_to(5)) { ... }
// ============================================================================

template <typename T>
class generator
{
public:
  struct promise_type
  {
    T current_value;
    std::exception_ptr exception;

    generator get_return_object()
    {
      return generator{std::coroutine_handle<promise_type>::from_promise(*this)};
    }

    std::suspend_always initial_suspend() noexcept { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }

    std::suspend_always yield_value(T value) noexcept
    {
      current_value = std::move(value);
      return {};
    }

    void return_void() noexcept {}

    void unhandled_exception() { exception = std::current_exception(); }

    void rethrow_if_exception()
    {
      if (exception)
        std::rethrow_exception(exception);
    }
  };

  using handle_type = std::coroutine_handle<promise_type>;

  // Iterator for range-based for
  class iterator
  {
  public:
    using iterator_category = std::input_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = T;
    using reference = T const&;
    using pointer = T const*;

    iterator() noexcept : m_handle(nullptr) {}
    explicit iterator(handle_type h) noexcept : m_handle(h) {}

    reference operator*() const noexcept { return m_handle.promise().current_value; }

    pointer operator->() const noexcept { return std::addressof(m_handle.promise().current_value); }

    iterator& operator++()
    {
      m_handle.resume();
      if (m_handle.done())
      {
        m_handle.promise().rethrow_if_exception();
        m_handle = nullptr;
      }
      return *this;
    }

    iterator operator++(int)
    {
      auto tmp = *this;
      ++(*this);
      return tmp;
    }

    bool operator==(std::default_sentinel_t) const noexcept { return !m_handle || m_handle.done(); }

    bool operator!=(std::default_sentinel_t s) const noexcept { return !(*this == s); }

  private:
    handle_type m_handle;
  };

  generator() noexcept : m_handle(nullptr) {}

  generator(generator&& other) noexcept : m_handle(other.m_handle) { other.m_handle = nullptr; }

  generator& operator=(generator&& other) noexcept
  {
    if (this != &other)
    {
      if (m_handle)
        m_handle.destroy();
      m_handle = other.m_handle;
      other.m_handle = nullptr;
    }
    return *this;
  }

  ~generator()
  {
    if (m_handle)
      m_handle.destroy();
  }

  generator(generator const&) = delete;
  generator& operator=(generator const&) = delete;

  iterator begin()
  {
    if (m_handle)
    {
      m_handle.resume();
      if (m_handle.done())
      {
        m_handle.promise().rethrow_if_exception();
        return iterator{};
      }
    }
    return iterator{m_handle};
  }

  std::default_sentinel_t end() noexcept { return {}; }

  // Manual iteration interface
  bool next()
  {
    if (!m_handle || m_handle.done())
      return false;
    m_handle.resume();
    return !m_handle.done();
  }

  T const& value() const noexcept { return m_handle.promise().current_value; }

  explicit operator bool() const noexcept { return m_handle && !m_handle.done(); }

private:
  explicit generator(handle_type h) noexcept : m_handle(h) {}
  handle_type m_handle;
};

// ============================================================================
// Frame timing state (shared with loop module)
// ============================================================================

namespace detail
{
inline double g_last_dt = 0.016; // ~60fps default
} // namespace detail

// ============================================================================
// frames() - Generator yielding dt each frame
//
// Usage:
//   for (double dt : frames(60) | std::views::take(100)) {
//     ball.pos += velocity * dt;
//   }
// ============================================================================

inline generator<double> frames(double fps = 60.0)
{
  using clock = std::chrono::steady_clock;
  using duration = std::chrono::duration<double>;

  double target_dt = 1.0 / fps;
  auto last_time = clock::now();

  while (true)
  {
    auto now = clock::now();
    double elapsed = duration(now - last_time).count();

    // Sleep if ahead of schedule
    if (elapsed < target_dt)
    {
      double sleep_time = target_dt - elapsed;
      if (sleep_time > 0.002)
        std::this_thread::sleep_for(std::chrono::duration<double>(sleep_time * 0.9));

      // Spin-wait for precision
      while (duration(clock::now() - last_time).count() < target_dt)
      {
      }

      now = clock::now();
      elapsed = duration(now - last_time).count();
    }

    last_time = now;
    detail::g_last_dt = elapsed;
    co_yield elapsed;
  }
}

// ============================================================================
// animate() - Time-bounded animation generator
//
// Yields (elapsed_time, dt) pairs for a fixed duration.
//
// Usage:
//   for (auto [t, dt] : animate(2.0, 60)) {  // 2 seconds at 60fps
//     double progress = t / 2.0;
//     object.opacity = 1.0 - progress;  // fade out
//   }
// ============================================================================

struct animation_frame
{
  double elapsed;  // total time since start
  double dt;       // time since last frame
  double progress; // elapsed / duration (0.0 to 1.0)
};

inline generator<animation_frame> animate(double duration_seconds, double fps = 60.0)
{
  double elapsed = 0.0;
  for (double dt : frames(fps))
  {
    if (elapsed >= duration_seconds)
      break;
    co_yield animation_frame{elapsed, dt, elapsed / duration_seconds};
    elapsed += dt;
  }
}

// ============================================================================
// iota() - Simple integer sequence generator
//
// Useful for indexed iteration.
// ============================================================================

inline generator<std::size_t> iota(std::size_t start, std::size_t end)
{
  for (std::size_t i = start; i < end; ++i)
    co_yield i;
}

inline generator<std::size_t> iota(std::size_t count)
{
  for (std::size_t i = 0; i < count; ++i)
    co_yield i;
}

// ============================================================================
// Frame Scheduler - Manages pending coroutines for callback-based loops
//
// In callback-based environments (like Emscripten's requestAnimationFrame),
// we need a scheduler to resume coroutines each frame rather than blocking.
// ============================================================================

class frame_scheduler
{
public:
  // Resume all pending coroutines for this frame
  void tick(double dt)
  {
    m_current_dt = dt;
    
    // Process all pending coroutines (swap to allow new ones during iteration)
    auto pending = std::move(m_pending);
    m_pending.clear();
    
    for (auto& handle : pending)
    {
      if (handle && !handle.done())
        handle.resume();
    }
  }
  
  // Called by next_frame awaitable to schedule resumption
  void schedule(std::coroutine_handle<> h)
  {
    m_pending.push_back(h);
  }
  
  double current_dt() const noexcept { return m_current_dt; }
  
  std::size_t pending_count() const noexcept { return m_pending.size(); }

private:
  std::vector<std::coroutine_handle<>> m_pending;
  double m_current_dt = 0.016;
};

// Global frame scheduler instance
inline frame_scheduler g_scheduler{};

// ============================================================================
// next_frame - Awaitable that suspends until the next frame
//
// Usage in a task coroutine:
//   task<void> animate_ball() {
//     while (ball.y > 0) {
//       ball.velocity.y += gravity * co_await next_frame();
//       ball.y += ball.velocity.y;
//     }
//   }
// ============================================================================

struct next_frame
{
  bool await_ready() const noexcept { return false; }
  
  void await_suspend(std::coroutine_handle<> h) const
  {
    g_scheduler.schedule(h);
  }
  
  double await_resume() const noexcept
  {
    return g_scheduler.current_dt();
  }
};

// ============================================================================
// task<T> - Async task for coroutine chaining
//
// A simple task type that can co_await other awaitables and return a value.
// Unlike generator, task is eager (starts immediately) and single-value.
//
// Usage:
//   task<void> raindrop_lifecycle(Drop& drop) {
//     // Fall phase
//     while (drop.pos.y > ground) {
//       double dt = co_await next_frame();
//       drop.velocity.y += gravity * dt;
//       drop.pos += drop.velocity * dt;
//     }
//     // Splat animation
//     for (int i = 0; i < 10; ++i) {
//       co_await next_frame();
//       drop.flatten();
//     }
//   }
// ============================================================================

template <typename T = void>
class task;

// Specialization for void
template <>
class task<void>
{
public:
  struct promise_type
  {
    std::exception_ptr exception;
    
    task get_return_object()
    {
      return task{std::coroutine_handle<promise_type>::from_promise(*this)};
    }
    
    std::suspend_never initial_suspend() noexcept { return {}; } // Eager start
    std::suspend_always final_suspend() noexcept { return {}; }
    
    void return_void() noexcept {}
    
    void unhandled_exception()
    {
      exception = std::current_exception();
    }
  };
  
  using handle_type = std::coroutine_handle<promise_type>;
  
  task() noexcept : m_handle(nullptr) {}
  
  task(task&& other) noexcept : m_handle(other.m_handle)
  {
    other.m_handle = nullptr;
  }
  
  task& operator=(task&& other) noexcept
  {
    if (this != &other)
    {
      if (m_handle)
        m_handle.destroy();
      m_handle = other.m_handle;
      other.m_handle = nullptr;
    }
    return *this;
  }
  
  ~task()
  {
    if (m_handle)
      m_handle.destroy();
  }
  
  task(task const&) = delete;
  task& operator=(task const&) = delete;
  
  bool done() const noexcept { return !m_handle || m_handle.done(); }
  
  explicit operator bool() const noexcept { return !done(); }
  
  // Detach: release ownership so coroutine lives independently
  void detach() noexcept { m_handle = nullptr; }

private:
  explicit task(handle_type h) noexcept : m_handle(h) {}
  handle_type m_handle;
};

// Non-void specialization (for tasks that return a value)
template <typename T>
class task
{
public:
  struct promise_type
  {
    T result;
    std::exception_ptr exception;
    
    task get_return_object()
    {
      return task{std::coroutine_handle<promise_type>::from_promise(*this)};
    }
    
    std::suspend_never initial_suspend() noexcept { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    
    void return_value(T value)
    {
      result = std::move(value);
    }
    
    void unhandled_exception()
    {
      exception = std::current_exception();
    }
  };
  
  using handle_type = std::coroutine_handle<promise_type>;
  
  task() noexcept : m_handle(nullptr) {}
  
  task(task&& other) noexcept : m_handle(other.m_handle)
  {
    other.m_handle = nullptr;
  }
  
  task& operator=(task&& other) noexcept
  {
    if (this != &other)
    {
      if (m_handle)
        m_handle.destroy();
      m_handle = other.m_handle;
      other.m_handle = nullptr;
    }
    return *this;
  }
  
  ~task()
  {
    if (m_handle)
      m_handle.destroy();
  }
  
  task(task const&) = delete;
  task& operator=(task const&) = delete;
  
  bool done() const noexcept { return !m_handle || m_handle.done(); }
  
  T const& result() const
  {
    if (m_handle.promise().exception)
      std::rethrow_exception(m_handle.promise().exception);
    return m_handle.promise().result;
  }
  
  explicit operator bool() const noexcept { return !done(); }
  
  void detach() noexcept { m_handle = nullptr; }

private:
  explicit task(handle_type h) noexcept : m_handle(h) {}
  handle_type m_handle;
};

// ============================================================================
// tick_coroutines() - Call each frame to resume pending coroutines
//
// Usage in update loop:
//   void update() {
//     vcpp::tick_coroutines(dt);  // Resume all pending tasks
//     // ... rest of update logic
//   }
// ============================================================================

inline void tick_coroutines(double dt = 0.016)
{
  g_scheduler.tick(dt);
}

} // namespace vcpp
