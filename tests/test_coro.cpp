/*
 *  test_coro.cpp - Unit tests for vcpp coroutine primitives
 */

import vcpp;
import std;

// Test generator basic iteration
bool test_generator_basic()
{
  auto gen = []() -> vcpp::generator<int> {
    for (int i = 0; i < 5; ++i)
      co_yield i;
  }();

  int sum = 0;
  int count = 0;
  for (int x : gen)
  {
    sum += x;
    ++count;
  }

  return sum == 10 && count == 5; // 0+1+2+3+4 = 10
}

// Test generator with move-only types
bool test_generator_move_only()
{
  auto gen = []() -> vcpp::generator<std::unique_ptr<int>> {
    for (int i = 0; i < 3; ++i)
      co_yield std::make_unique<int>(i * 10);
  }();

  int sum = 0;
  for (auto const& ptr : gen)
    sum += *ptr;

  return sum == 30; // 0 + 10 + 20
}

// Test iota generator
bool test_iota()
{
  int sum = 0;
  for (auto i : vcpp::iota(5))
    sum += static_cast<int>(i);
  return sum == 10;
}

// Test iota with range
bool test_iota_range()
{
  int sum = 0;
  for (auto i : vcpp::iota(3, 7))
    sum += static_cast<int>(i);
  return sum == 18; // 3+4+5+6
}

// Test frames generator (just verify it compiles and yields)
bool test_frames_compiles()
{
  auto frame_gen = vcpp::frames(60.0);

  // Just take first frame to verify it works
  int count = 0;
  for (double dt : frame_gen)
  {
    if (dt > 0.0)
      ++count;
    if (count >= 2)
      break;
  }
  return count >= 2;
}

// Test animate generator
bool test_animate_compiles()
{
  // Very short animation for testing
  auto anim = vcpp::animate(0.05, 60.0); // 50ms at 60fps

  int count = 0;
  for (auto frame : anim)
  {
    if (frame.dt > 0 && frame.progress >= 0.0 && frame.progress <= 1.0)
      ++count;
    if (count >= 2)
      break; // Don't wait for full duration in test
  }
  return count >= 1;
}

// A task can co_await another and get its value; exceptions cross the await
bool test_task_awaits_task()
{
  auto child = []() -> vcpp::task<int> {
    co_await vcpp::next_frame();
    co_return 7;
  };
  auto failing = []() -> vcpp::task<void> {
    co_await vcpp::next_frame();
    throw std::runtime_error("boom");
  };
  int got = 0;
  bool caught = false;
  // Named, not invoked in place: the coroutine refers to the lambda, which must outlive it
  auto parent_fn = [&]() -> vcpp::task<void> {
    got = co_await child();
    try
    {
      co_await failing();
    }
    catch (const std::runtime_error&)
    {
      caught = true;
    }
  };
  auto parent = parent_fn();
  for (int i = 0; i < 4 && !parent.done(); ++i)
    vcpp::tick_coroutines();
  return parent.done() && got == 7 && caught;
}

// Drive the scheduler like a 60 fps frame loop for `seconds` of real time
void run_frames(double seconds)
{
  auto end = std::chrono::steady_clock::now() + std::chrono::duration<double>(seconds);
  while (std::chrono::steady_clock::now() < end)
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
    vcpp::tick_coroutines();
  }
}

// Coroutine that counts iterations of `step` until stopped
vcpp::task<void> count_loop(int& n, bool& stop, auto step)
{
  while (!stop)
  {
    co_await step();
    ++n;
  }
}

// 10 ms waits run ~100 times a second although frames come every ~16 ms
bool test_wait_for_rate()
{
  int n = 0;
  bool stop = false;
  auto t = count_loop(n, stop, [] { return vcpp::wait_for(0.01); });
  run_frames(0.5);
  stop = true;
  return n >= 40 && n <= 60;
}

// GlowScript's rate(): rate(30) waits ceil(1000/30) = 34 ms, so ~29/s. rate(200) runs ceil(200/60) = 4
// iterations per ceil(1000/60) = 17 ms window, so ~235/s in GlowScript too, not 200.
bool test_rate()
{
  int slow = 0;
  int fast = 0;
  bool stop = false;
  auto a = count_loop(slow, stop, [] { return vcpp::rate(30); });
  run_frames(0.5);
  stop = true;
  run_frames(0.1);
  stop = false;
  auto b = count_loop(fast, stop, [] { return vcpp::rate(200); });
  run_frames(0.5);
  stop = true;
  return slow >= 12 && slow <= 17 && fast >= 100 && fast <= 135;
}

int main()
{
  int passed = 0;
  int failed = 0;

  auto run_test = [&](const char* name, bool (*test)()) {
    bool result = test();
    if (result)
    {
      std::cout << "  ✓ " << name << "\n";
      ++passed;
    }
    else
    {
      std::cout << "  ✗ " << name << " FAILED\n";
      ++failed;
    }
  };

  std::cout << "vcpp::coro tests\n";
  std::cout << "================\n";

  run_test("generator basic iteration", test_generator_basic);
  run_test("generator move-only types", test_generator_move_only);
  run_test("iota(n)", test_iota);
  run_test("iota(start, end)", test_iota_range);
  run_test("frames() compiles and yields", test_frames_compiles);
  run_test("animate() compiles and yields", test_animate_compiles);
  run_test("task awaits task", test_task_awaits_task);
  run_test("wait_for(10 ms) ~100/s", test_wait_for_rate);
  run_test("rate(30) and rate(200)", test_rate);

  std::cout << "================\n";
  std::cout << "Passed: " << passed << "/" << (passed + failed) << "\n";

  return failed > 0 ? 1 : 0;
}
