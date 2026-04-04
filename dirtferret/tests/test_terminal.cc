// ============================================================================
// TEST: Terminal Class
// ============================================================================
//
// These tests verify that your Terminal class has the correct interface and
// basic behavior. They WON'T COMPILE until you implement Terminal in
// frontend/terminal.h and frontend/terminal.cc.
//
// WHAT THESE TESTS FORCE YOU TO IMPLEMENT:
//   - A constructor that calls setup (or an explicit setup method)
//   - A destructor that calls teardown (RAII)
//   - cols(), rows(), pixel_width(), pixel_height() accessors
//   - refresh_size() method
//   - Non-copyable (deleted copy ctor and assignment)
//
// NOTE ON TESTING TERMINAL STATE:
//   Some tests check actual terminal dimensions. These will SKIP if
//   stdout isn't a real terminal (e.g., in a pipe or CI). The important
//   thing is that they COMPILE — that proves your interface is correct.
//   Run them in your actual terminal to verify behavior.
//
// ============================================================================
#include <gtest/gtest.h>

#include "frontend/terminal.h"

#include <type_traits>
#include <unistd.h>

using namespace cef_terminal;

// --- Compile-time interface checks ---

// Terminal must NOT be copyable. If you forgot to delete the copy
// constructor or copy assignment, these static_asserts will fail at
// compile time with a clear error.
static_assert(!std::is_copy_constructible_v<Terminal>,
              "Terminal must not be copy-constructible (delete the copy ctor)");
static_assert(!std::is_copy_assignable_v<Terminal>,
              "Terminal must not be copy-assignable (delete copy assignment)");

// --- Runtime tests ---

class TerminalTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Skip all runtime tests if we're not attached to a real terminal.
    // The compile-time checks above still run regardless.
    if (!isatty(STDOUT_FILENO)) {
      GTEST_SKIP() << "Not a real terminal (stdout is not a tty). "
                   << "Run tests from an interactive terminal to "
                   << "exercise Terminal behavior.";
    }
  }
};

TEST_F(TerminalTest, ConstructAndDestruct) {
  // Constructing a Terminal should set up raw mode + alt screen.
  // Destructing it should tear everything down cleanly.
  // If this crashes or leaves the terminal in a bad state, your
  // RAII teardown is broken.
  {
    auto t = cef_terminal::Terminal::create();
    // Just let it go out of scope — destructor must restore state.
  }
  // If we get here without the terminal being hosed, teardown works.
}

TEST_F(TerminalTest, DimensionsArePopulated) {
  auto term = cef_terminal::Terminal::create();

  // After construction, character dimensions must be positive.
  // A terminal always has at least 1 column and 1 row.
  EXPECT_GT(term->cols(), 0)
      << "cols() must return a positive value after setup";
  EXPECT_GT(term->rows(), 0)
      << "rows() must return a positive value after setup";
}

TEST_F(TerminalTest, PixelDimensionsNonNegative) {
  auto term = cef_terminal::Terminal::create();

  // Pixel dimensions might be 0 if the terminal doesn't report them
  // (your code should have a fallback), but they must never be negative.
  EXPECT_GE(term->pixel_width(), 0) << "pixel_width() must be non-negative";
  EXPECT_GE(term->pixel_height(), 0) << "pixel_height() must be non-negative";
}

TEST_F(TerminalTest, RefreshSizeIsCallable) {
  auto term = cef_terminal::Terminal::create();
  int old_cols = term->cols();
  int old_rows = term->rows();

  // refresh_size() must not crash. We can't easily change the terminal
  // size in a test, but we can verify it doesn't blow up and returns
  // consistent values when the size hasn't changed.
  term->refresh_size();

  EXPECT_EQ(term->cols(), old_cols);
  EXPECT_EQ(term->rows(), old_rows);
}

TEST_F(TerminalTest, DoubleDestructIsSafe) {
  // Edge case: if teardown has a guard (setup_done_ flag), destroying
  // twice shouldn't crash. We can't easily call the destructor twice,
  // but we can test that constructing in a nested scope is fine.
  {
    auto term2 = cef_terminal::Terminal::create();
    {
      auto term2 = cef_terminal::Terminal::create();
      // term2 destructor fires here
    }
    // term1 destructor fires here
    // Both should restore state without crashing.
    // (In practice, term2's teardown restores term1's saved state,
    // which is the raw-mode state. Then term1 restores the original.
    // This is a known limitation of nested Terminal instances —
    // don't do it in production, but it shouldn't crash.)
  }
}
