#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace cef_terminal {

// Forward declarations for handler function types.
// Defined here (not in dispatcher.h) so exports.h can use them without
// pulling in the dispatcher.
struct Command;
struct Query;
struct CommandResult;
struct QueryResult;

// Value type for command/query arguments and results.
// Covers all common payload types. Binary data (e.g., pixel buffers)
// should travel via IPC transport's efficient path, not through args.
using Value = std::variant<
    std::monostate,         // null / no value
    bool,
    int64_t,
    double,
    std::string
>;

using Args = std::unordered_map<std::string, Value>;

// Result of a command execution.
struct CommandResult {
    bool ok = true;
    std::string error;      // non-empty when ok == false

    static CommandResult success() { return {true, {}}; }
    static CommandResult failure(std::string msg) { return {false, std::move(msg)}; }
};

// Result of a query execution.
struct QueryResult {
    bool ok = true;
    std::string error;
    Args data;              // returned key-value pairs

    static QueryResult success(Args d = {}) { return {true, {}, std::move(d)}; }
    static QueryResult failure(std::string msg) { return {false, std::move(msg), {}}; }
};

// Handler function types — used by both the Dispatcher and ExportManifest.
using CommandHandler = std::function<CommandResult(const Command&)>;
using QueryHandler = std::function<QueryResult(const Query&)>;

}  // namespace cef_terminal
