// ============================================================================
// Terminal implementation
// See terminal.h for the full task description.
// ============================================================================
#include "terminal.h"

// #include <csignal>
#include <iostream>
// #include <memory>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

namespace cef_terminal {

struct termios original;

// See terminal.h line 150
// std::unique_ptr<Terminal> Terminal::create() {
Terminal *Terminal::create() {
  // auto t = std::unique_ptr<Terminal>(new Terminal());
  Terminal *t = new Terminal();
  if (!t->setup()) {
    return nullptr;
  }
  return t;
}

bool Terminal::setup() {

  // Alternate Buffer
  std::cout << "\033[?1049h";

  // Hide cursor
  std::cout << "\033[?25l";

  // Set Raw and flags
  if (tcgetattr(STDIN_FILENO, &original_) != 0)
    return false;
  else {
    raw_ = original_;
    raw_.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);

    raw_.c_iflag &= ~(IXON | ICRNL);

    raw_.c_cc[VMIN] = 0;
    raw_.c_cc[VTIME] = 0;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_);

    setup_done_ = true;
    return true;
  }
}

void Terminal::teardown() { tcsetattr(STDIN_FILENO, TCSANOW, &original_); }

void refresh_size() {}

} // namespace cef_terminal
