#pragma once

#include "api/types.h"
#include <string>

namespace cef_terminal {

// A command is a fire-and-forget action that crosses layer boundaries.
// Commands may modify state. They carry a name (dot-separated namespace)
// and optional metadata args.
//
// Examples: "buffer.navigate", "buffer.close", "frontend.render"
struct Command {
    std::string name;
    Args args;

    Command() = default;
    Command(std::string n, Args a = {})
        : name(std::move(n)), args(std::move(a)) {}
};

}  // namespace cef_terminal
