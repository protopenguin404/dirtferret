#pragma once

#include "api/command.h"
#include "api/query.h"
#include "api/types.h"

#include <functional>
#include <string>
#include <unordered_map>

namespace cef_terminal {

// Handler function types.
using CommandHandler = std::function<CommandResult(const Command&)>;
using QueryHandler = std::function<QueryResult(const Query&)>;

// The Dispatcher is the central routing hub of the API layer.
// All cross-layer communication goes through here — no exceptions.
//
// Layers register handlers for command/query names. When a command or
// query is dispatched, the dispatcher looks up the handler by name and
// invokes it. If no handler is registered, it returns an error result.
class Dispatcher {
 public:
    void register_command(const std::string& name, CommandHandler handler);
    void register_query(const std::string& name, QueryHandler handler);

    CommandResult dispatch(const Command& cmd) const;
    QueryResult dispatch(const Query& query) const;

    bool has_command(const std::string& name) const;
    bool has_query(const std::string& name) const;

 private:
    std::unordered_map<std::string, CommandHandler> command_handlers_;
    std::unordered_map<std::string, QueryHandler> query_handlers_;
};

}  // namespace cef_terminal
