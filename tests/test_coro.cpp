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

  std::cout << "================\n";
  std::cout << "Passed: " << passed << "/" << (passed + failed) << "\n";

  return failed > 0 ? 1 : 0;
}
