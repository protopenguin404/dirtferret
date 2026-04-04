// ============================================================================
// LEARNING TASK: Terminal
// ============================================================================
//
// PURPOSE:
//   Manages terminal state for the frontend process. Handles setup/teardown
//   of raw mode, alternate screen buffer, and size detection. This is the
//   low-level terminal plumbing that KittyRenderer and (eventually) input
//   handling both depend on.
//
// ---- WHAT THIS CLASS NEEDS TO DO ----
//
// setup():
//   1. Switch to the ALTERNATE SCREEN BUFFER.
//      Write: \033[?1049h  to stdout
//      This gives us a clean canvas that disappears when we exit, restoring
//      whatever was on screen before. Same thing vim/less/etc. do.
//
//   2. HIDE THE CURSOR.
//      Write: \033[?25l
//
//   3. Put the terminal into RAW MODE via termios.
//      HINT: tcgetattr(STDIN_FILENO, &original_) to save the original state.
//      Then modify a copy:
//        - Turn OFF: ECHO, ICANON, ISIG, IEXTEN  (from c_lflag)
//        - Turn OFF: IXON, ICRNL               (from c_iflag)
//        - Set VMIN=0, VTIME=0                 (non-blocking reads)
//      Then tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw)
//      Header: <termios.h>
//
//      WHY RAW MODE: Canonical mode buffers input until newline, echoes
//      typed characters, and interprets Ctrl-C as SIGINT. We need none of
//      that — we want every keypress immediately, with no echo, and we'll
//      handle signals ourselves.
//
//      PITFALL: If you crash without restoring termios, the user's terminal
//      is hosed (no echo, no line editing). That's why teardown() matters
//      and why signal handling matters.
//
/////////////////////////////////////////////////////////////////////
//
//            Example from termios docs:
//
//              The following attributes changes the terminal attributes.
//
//            *⁄
//           #define _POSIX_SOURCE
//           #include <termios.h>
//           #include <unistd.h>
//           #include <stdio.h>
//
//           main() {
//             struct termios term1, term2;
//
//             if (tcgetattr(STDIN_FILENO, &term1) != 0)
//               perror("tcgetattr() error");
//             else {
//               printf("the original end-of-file character is x'%02x'\n",
//                      term1.c_cc[VEOF]);
//               term1.c_cc[VEOF] = 'z';
//               if (tcsetattr(STDIN_FILENO, TCSANOW, &term1) != 0)
//                 perror("tcsetattr() error");
//               if (tcgetattr(STDIN_FILENO, &term1) != 0)
//                 perror("tcgetattr() error");
//               else
//                 printf("the new end-of-file character is x'%02x'\n",
//                        term1.c_cc[VEOF]);
//             }
//           }
//
////////////////////////////////////////////////////////////////////////////
///
//   4. Query terminal size and store it.
//      HINT: ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws)
//      The winsize struct has: ws_col, ws_row, ws_xpixel, ws_ypixel
//      Store all four — KittyRenderer needs pixel dimensions.
//
// teardown():
//   The EXACT REVERSE of setup, in reverse order:
//   1. Restore original termios: tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_)
//   2. Show cursor: \033[?25h
//   3. Leave alternate screen: \033[?1049l
//
//   CRITICAL: The destructor MUST call teardown(). If the process exits
//   without restoring the terminal, the user is left in raw mode with no
//   echo. Defense in depth: also install a signal handler for SIGINT/SIGTERM
//   that calls teardown (or use std::atexit).
//
//   RAII PATTERN: setup() in constructor (or explicit init), teardown() in
//   destructor. The destructor is your safety net. This is one of C++'s
//   biggest strengths — deterministic cleanup. Read about RAII if you
//   haven't already; it's the single most important C++ idiom.
//
// refresh_size():
//   Re-query terminal dimensions. Call this when you receive SIGWINCH
//   (terminal was resized). For now, just call it on demand.
//
// ---- SUGGESTED INTERFACE ----
//
//   Terminal();                   // calls setup()
//   ~Terminal();                  // calls teardown()
//   Terminal(const Terminal&) = delete;             // non-copyable
//   Terminal& operator=(const Terminal&) = delete;  // non-copyable
//
//   int cols() const;
//   int rows() const;
//   int pixel_width() const;
//   int pixel_height() const;
//   void refresh_size();
//
// Private:
//   struct termios original_;    // saved terminal state for restore
//   int cols_, rows_;
//   int pixel_w_, pixel_h_;
//   bool setup_done_ = false;   // guard against double-teardown
//
// ---- ARCHITECTURAL CONTEXT ----
//
//   - Created early in frontend_main.cc, before KittyRenderer.
//   - KittyRenderer may take a const reference to Terminal to read
//     dimensions, or you pass dimensions explicitly — your call.
//   - Eventually, input handling will also use Terminal (reading from
//     stdin in raw mode). For now, just get setup/teardown right.
//   - This class is frontend-only. No CEF, no IPC awareness.
//
// ---- SIGNAL HANDLING NOTE ----
//
//   Eventually you'll want SIGWINCH (resize) and SIGINT/SIGTERM (cleanup).
//   For now, a simple approach: store a global pointer to the Terminal
//   instance, install a signal handler that calls teardown() then _exit().
//   This is a known pattern for terminal apps. Not beautiful, but correct.
//   HINT: <csignal>, std::signal() or sigaction()
//
// ---- CPP THINGS YOU'LL WANT TO READ UP ON ----
//
//   - struct termios, tcgetattr, tcsetattr — POSIX terminal control
//   - ioctl(2) + TIOCGWINSZ — terminal size query
//   - RAII — Resource Acquisition Is Initialization
//   - = delete on copy constructor/assignment — preventing copies
//   - std::atexit — registering cleanup functions
//   - write(STDOUT_FILENO, ...) — unbuffered output for escape sequences
//
// ============================================================================
#pragma once

// Why do they flip the 'l' and 'h' here? so confusing lol
#define CRSR_HIDE "\033[?25l"
#define CRSR_SHOW "\033[?25h"
#define CRSR_HOME "\033[H"
#define ENTR_ALTRN_BUF "\033[?1049h"
#define EXT_ALTRN_BUF "\033[?1049l"
#define CLS "\033[2J"

#include <memory>
#include <termios.h>

namespace cef_terminal {

class Terminal {
public:
  Terminal(const Terminal &) = delete;            // non-copyable
  Terminal &operator=(const Terminal &) = delete; // non-copyable

  static std::unique_ptr<Terminal, void (*)(Terminal *)> create();
  static void destroy(Terminal *t);
  void refresh_size();

  int cols() const { return cols_; }
  int rows() const { return rows_; }
  int pixel_height() const { return pixel_h_; }
  int pixel_width() const { return pixel_w_; }

private:
  Terminal() = default;
  ~Terminal() = default;
  bool setup();
  struct termios original_;
  struct termios raw_;
  int cols_;
  int rows_;
  int pixel_w_;
  int pixel_h_;
  bool setup_done_;
};

} // namespace cef_terminal
