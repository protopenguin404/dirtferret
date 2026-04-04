#pragma once

#include "api/types.h"
#include <string>

namespace cef_terminal {

// A query is a read-only request that crosses layer boundaries.
// Queries MUST NOT modify state — they only return information.
//
// Examples: "buffer.get_url", "buffer.list", "frontend.get_dimensions"
struct Query {
    std::string name;
    Args args;

    Query() = default;
    Query(std::string n, Args a = {})
        : name(std::move(n)), args(std::move(a)) {}
};

}  // namespace cef_terminal
