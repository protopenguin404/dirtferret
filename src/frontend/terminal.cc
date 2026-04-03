// ============================================================================
// Terminal implementation
// See terminal.h for the full task description.
// ============================================================================
#include "terminal.h"

#include <csignal>
#include <iostream>
#include <memory>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

namespace cef_terminal {

std::unique_ptr<Terminal, void (*)(Terminal *)> Terminal::create() {
  auto t = std::unique_ptr<Terminal, void (*)(Terminal *)>(new Terminal,
                                                           &Terminal::destroy);
  if (!t->setup()) {
    return {nullptr, &Terminal::destroy};
  }
  return t;
}

bool Terminal::setup() {

  std::cout << CLS;
  std::cout << ENTR_ALTRN_BUF;
  std::cout << CRSR_HOME;
  std::cout << CRSR_HIDE;

  std::cout.flush();

  // Set Raw and flags
  if (tcgetattr(STDIN_FILENO, &original_) != 0) {
    return false;
  } else {

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

void Terminal::destroy(Terminal *t) {

  // Reset original term state
  tcsetattr(STDIN_FILENO, TCSANOW, &t->original_);
  std::cout << CRSR_SHOW;
  std::cout << EXT_ALTRN_BUF;
  std::cout.flush();

  delete t;
}

void Terminal::refresh_size() {
  cols_ = 0;
  rows_ = 0;
}

} // namespace cef_terminal
