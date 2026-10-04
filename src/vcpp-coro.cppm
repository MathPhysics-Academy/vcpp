/*
 *  vcpp:coro - C++20 Coroutine primitives for animation
 *
 *  Provides:
 *  - generator<T>: Lazy sequence generator (range-for compatible)
 *  - frames(fps): Generator yielding dt each frame
 *  - animate(duration, fps): Time-bounded animation generator
 *  - task<T>: eager coroutine; co_await one task from another
 *  - task_scope: owns tasks for its own lifetime; their frames come from its pool
 *  - next_frame, wait_for(seconds): awaitables resumed by tick_coroutines()
 *  - rate, sleep: GlowScript's timing, ported
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

    // Resume this frame's batch; ones scheduled meanwhile wait for the next tick. cancel() can
    // null out entries of the batch, when a resumed coroutine destroys a task waiting in it.
    m_batch = std::move(m_pending);
    m_pending.clear();
    for (std::size_t i = 0; i < m_batch.size(); ++i)
    {
      if (auto h = std::exchange(m_batch[i], nullptr); h && !h.done())
        h.resume();
    }
    m_batch.clear();

    // Timers run in deadline order, each seeing now() == its own deadline, so a coroutine that
    // sleeps 10 ms runs ~100 times a second even though frames come every ~16 ms. A coroutine
    // more than max_lag behind is moved up to the wall clock instead of catching up.
    const double wall = wall_seconds();
    while (!m_timers.empty() && m_timers.front().due <= wall)
    {
      std::pop_heap(m_timers.begin(), m_timers.end(), timer::later);
      timer t = m_timers.back();
      m_timers.pop_back();
      m_virtual_now = std::max(t.due, wall - max_lag);
      t.handle.resume();
    }
    m_virtual_now.reset();
  }
  
  // Called by next_frame awaitable to schedule resumption
  void schedule(std::coroutine_handle<> h)
  {
    m_pending.push_back(h);
  }

  // Called by wait_for to resume h once now() reaches due
  void schedule_at(double due, std::coroutine_handle<> h)
  {
    m_timers.push_back({due, m_next_seq++, h});
    std::push_heap(m_timers.begin(), m_timers.end(), timer::later);
  }

  // Forget h: its coroutine is being destroyed and must not be resumed
  void cancel(std::coroutine_handle<> h)
  {
    std::erase(m_pending, h);
    std::ranges::replace(m_batch, h, std::coroutine_handle<>{});
    if (std::erase_if(m_timers, [h](const timer& t) { return t.handle == h; }))
      std::make_heap(m_timers.begin(), m_timers.end(), timer::later);
  }

  // Seconds on a steady clock; inside a timer's resumption, that timer's deadline
  double now() const { return m_virtual_now ? *m_virtual_now : wall_seconds(); }

  double current_dt() const noexcept { return m_current_dt; }

  std::size_t pending_count() const noexcept { return m_pending.size() + m_timers.size(); }

private:
  struct timer
  {
    double due;
    std::uint64_t seq; // FIFO among equal deadlines
    std::coroutine_handle<> handle;
    static bool later(const timer& a, const timer& b) noexcept
    { return a.due != b.due ? a.due > b.due : a.seq > b.seq; }
  };

  static double wall_seconds()
  { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

  static constexpr double max_lag = 0.25;

  std::vector<std::coroutine_handle<>> m_pending;
  std::vector<std::coroutine_handle<>> m_batch; // being resumed by tick()
  std::vector<timer> m_timers;                  // min-heap on (due, seq)
  std::optional<double> m_virtual_now;
  std::uint64_t m_next_seq = 0;
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
// wait_for - Awaitable that suspends for a duration
//
// Resumed by tick_coroutines(), so the wait is at least `seconds` and ends on a frame. Waits
// shorter than 4 ms count as 4 ms, as browsers clamp setTimeout; without a floor, a loop of
// zero-length waits would never leave tick().
//
//   co_await wait_for(0.5);
// ============================================================================

struct [[nodiscard]] wait_for
{
  double seconds;

  bool await_ready() const noexcept { return false; }
  void await_suspend(std::coroutine_handle<> h) const
  { g_scheduler.schedule_at(g_scheduler.now() + std::max(seconds, 0.004), h); }
  void await_resume() const noexcept {}
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

namespace detail
{
// A finished task hands control back to whoever co_awaited it (symmetric transfer).
struct resume_continuation
{
  bool await_ready() const noexcept { return false; }
  template<typename Promise>
  std::coroutine_handle<> await_suspend(std::coroutine_handle<Promise> h) const noexcept
  {
    if (auto c = h.promise().continuation)
      return c;
    return std::noop_coroutine();
  }
  void await_resume() const noexcept {}
};

// Where a task's frame is allocated: the pool of the task_scope whose spawn() is creating it, else
// the heap. A prefix in front of the frame records which, so operator delete returns it there.
inline thread_local std::pmr::memory_resource* g_frame_resource = nullptr;

struct frame_allocation
{
  static constexpr std::size_t prefix = alignof(std::max_align_t); // keeps the frame aligned

  static void* allocate(std::size_t size)
  {
    std::pmr::memory_resource* r = g_frame_resource;
    void* p = r ? r->allocate(size + prefix, alignof(std::max_align_t)) : ::operator new(size + prefix);
    *static_cast<std::pmr::memory_resource**>(p) = r;
    return static_cast<std::byte*>(p) + prefix;
  }

  static void deallocate(void* frame, std::size_t size) noexcept
  {
    void* p = static_cast<std::byte*>(frame) - prefix;
    if (auto* r = *static_cast<std::pmr::memory_resource**>(p))
      r->deallocate(p, size + prefix, alignof(std::max_align_t));
    else
      ::operator delete(p, size + prefix);
  }
};
} // namespace detail

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
    std::coroutine_handle<> continuation; // the coroutine co_awaiting this one, if any

    // Only the frame spawn() creates goes to its scope; anything this task starts uses the heap.
    promise_type() noexcept { detail::g_frame_resource = nullptr; }

    static void* operator new(std::size_t size) { return detail::frame_allocation::allocate(size); }
    static void operator delete(void* p, std::size_t size) noexcept { detail::frame_allocation::deallocate(p, size); }

    task get_return_object()
    {
      return task{std::coroutine_handle<promise_type>::from_promise(*this)};
    }
    
    std::suspend_never initial_suspend() noexcept { return {}; } // Eager start
    detail::resume_continuation final_suspend() noexcept { return {}; }

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
      destroy();
      m_handle = other.m_handle;
      other.m_handle = nullptr;
    }
    return *this;
  }

  ~task() { destroy(); }

  task(task const&) = delete;
  task& operator=(task const&) = delete;
  
  bool done() const noexcept { return !m_handle || m_handle.done(); }
  
  explicit operator bool() const noexcept { return !done(); }
  
  // Detach: release ownership so coroutine lives independently
  void detach() noexcept { m_handle = nullptr; }

  // An exception that ended the task, rethrown; nothing if none
  void rethrow_if_exception() const
  {
    if (m_handle && m_handle.promise().exception)
      std::rethrow_exception(m_handle.promise().exception);
  }

  auto operator co_await() const noexcept
  {
    struct awaiter
    {
      handle_type h;
      bool await_ready() const noexcept { return !h || h.done(); }
      void await_suspend(std::coroutine_handle<> c) const noexcept { h.promise().continuation = c; }
      void await_resume() const
      {
        if (h && h.promise().exception)
          std::rethrow_exception(h.promise().exception);
      }
    };
    return awaiter{m_handle};
  }

private:
  explicit task(handle_type h) noexcept : m_handle(h) {}

  void destroy()
  {
    if (m_handle)
    {
      g_scheduler.cancel(m_handle);
      m_handle.destroy();
    }
  }

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
    std::coroutine_handle<> continuation;

    promise_type() noexcept(std::is_nothrow_default_constructible_v<T>) { detail::g_frame_resource = nullptr; }

    static void* operator new(std::size_t size) { return detail::frame_allocation::allocate(size); }
    static void operator delete(void* p, std::size_t size) noexcept { detail::frame_allocation::deallocate(p, size); }

    task get_return_object()
    {
      return task{std::coroutine_handle<promise_type>::from_promise(*this)};
    }
    
    std::suspend_never initial_suspend() noexcept { return {}; }
    detail::resume_continuation final_suspend() noexcept { return {}; }

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
      destroy();
      m_handle = other.m_handle;
      other.m_handle = nullptr;
    }
    return *this;
  }

  ~task() { destroy(); }

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

  auto operator co_await() const noexcept
  {
    struct awaiter
    {
      handle_type h;
      bool await_ready() const noexcept { return h.done(); }
      void await_suspend(std::coroutine_handle<> c) const noexcept { h.promise().continuation = c; }
      T await_resume() const
      {
        if (h.promise().exception)
          std::rethrow_exception(h.promise().exception);
        return std::move(h.promise().result);
      }
    };
    return awaiter{m_handle};
  }

private:
  explicit task(handle_type h) noexcept : m_handle(h) {}

  void destroy()
  {
    if (m_handle)
    {
      g_scheduler.cancel(m_handle);
      m_handle.destroy();
    }
  }

  handle_type m_handle;
};

// ============================================================================
// task_scope - Owns tasks for as long as it exists
//
// spawn() calls a coroutine and keeps its task; the frame comes from the scope's pool. Destroying
// the scope destroys every task it holds: wake-ups are cancelled, locals' destructors run, and the
// memory goes back to the pool, which is released with the scope. Finished tasks are dropped on
// the next spawn(), so a long-lived scope stays bounded. A scope must not be destroyed from inside
// one of its own tasks.
//
//   vcpp::task_scope rain;               // or task_scope rain(&my_resource);
//   rain.spawn(raindrop, idx, x, z);
// ============================================================================

class task_scope
{
public:
  // The pool takes its blocks from upstream: the default heap, or a resource you supply.
  explicit task_scope(std::pmr::memory_resource* upstream = std::pmr::get_default_resource()) : m_pool(upstream) {}

  template<typename F, typename... Args>
    requires std::same_as<std::invoke_result_t<F, Args...>, task<void>>
  void spawn(F&& f, Args&&... args)
  {
    reap();
    auto* previous = std::exchange(detail::g_frame_resource, &m_pool);
    try
    {
      m_tasks.push_back(std::invoke(std::forward<F>(f), std::forward<Args>(args)...));
    }
    catch (...)
    {
      detail::g_frame_resource = previous;
      throw;
    }
    detail::g_frame_resource = previous;
  }

  // Tasks held, finished ones included until the next spawn()
  std::size_t size() const noexcept { return m_tasks.size(); }

private:
  // Drop finished tasks; an exception that ended one is reported, not lost
  void reap()
  {
    std::erase_if(m_tasks, [](const task<void>& t) {
      if (!t.done())
        return false;
      try
      {
        t.rethrow_if_exception();
      }
      catch (const std::exception& e)
      {
        std::println("task_scope: a task ended with an exception: {}", e.what());
      }
      catch (...)
      {
        std::println("task_scope: a task ended with an exception");
      }
      return true;
    });
  }

  std::pmr::unsynchronized_pool_resource m_pool; // declared first, so destroyed after the tasks
  std::vector<task<void>> m_tasks;
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

// ============================================================================
// rate, sleep - GlowScript's timing functions, with GlowScript's semantics
// ============================================================================


// sleep(dt): wait dt seconds (GlowScript's sleep is a setTimeout)
inline wait_for sleep(double seconds) { return wait_for{seconds}; }

// The result of rate(): either continue at once or wait.
struct [[nodiscard]] rate_wait
{
  std::optional<double> seconds;

  bool await_ready() const noexcept { return !seconds; }
  void await_suspend(std::coroutine_handle<> h) const { wait_for{*seconds}.await_suspend(h); }
  void await_resume() const noexcept {}
};

namespace detail
{
inline constexpr double rate_desired_fps = 60;
inline int rate_iters_left = 0; // GlowScript's N, shared by every caller as in GlowScript
inline double rate_end_ms = 0;  // GlowScript's enditers
inline double rate_msclock() { return 1000.0 * g_scheduler.now(); }
} // namespace detail

// rate(iters): GlowScript's rate() without the callback form (WebGLRenderer.js, rate()).
// At most 120 it waits ceil(1000/iters) ms. Above that it runs ceil(iters/60) iterations back to
// back, then waits out the rest of that 1/60 s.
inline rate_wait rate(double iters)
{
  using namespace detail;
  if (rate_iters_left > 0)
  {
    --rate_iters_left;
    const double timer = rate_msclock();
    if (timer > rate_end_ms)
      rate_iters_left = 1; // truncate the iterations to permit renders to occur
    if (rate_iters_left > 1)
      return {};
    rate_iters_left = 0;
    double dt = rate_end_ms - std::ceil(timer);
    if (dt < 5)
      dt = 0;
    return {dt / 1000};
  }
  if (iters <= 120)
    return {std::ceil(1000 / iters) / 1000};
  rate_iters_left = static_cast<int>(std::ceil(iters / rate_desired_fps));
  rate_end_ms = rate_msclock() + std::ceil(1000 / rate_desired_fps);
  return {};
}

} // namespace vcpp
