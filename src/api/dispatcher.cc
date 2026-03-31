#include "api/dispatcher.h"
#include <iostream>

namespace cef_terminal {

void Dispatcher::register_command(const std::string& name, CommandHandler handler) {
    command_handlers_[name] = std::move(handler);
}

void Dispatcher::register_query(const std::string& name, QueryHandler handler) {
    query_handlers_[name] = std::move(handler);
}

CommandResult Dispatcher::dispatch(const Command& cmd) const {
    auto it = command_handlers_.find(cmd.name);
    if (it == command_handlers_.end()) {
        std::cerr << "[api] No handler for command: " << cmd.name << std::endl;
        return CommandResult::failure("unknown command: " + cmd.name);
    }
    return it->second(cmd);
}

QueryResult Dispatcher::dispatch(const Query& query) const {
    auto it = query_handlers_.find(query.name);
    if (it == query_handlers_.end()) {
        std::cerr << "[api] No handler for query: " << query.name << std::endl;
        return QueryResult::failure("unknown query: " + query.name);
    }
    return it->second(query);
}

bool Dispatcher::has_command(const std::string& name) const {
    return command_handlers_.count(name) > 0;
}

bool Dispatcher::has_query(const std::string& name) const {
    return query_handlers_.count(name) > 0;
}

}  // namespace cef_terminal
