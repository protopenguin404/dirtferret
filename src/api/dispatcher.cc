#include "api/dispatcher.h"
#include <iostream>

namespace cef_terminal {

void Dispatcher::register_exports(ExportManifest manifest) {
    // Lazily register introspection queries on first export registration.
    if (!introspection_registered_) {
        register_introspection();
        introspection_registered_ = true;
    }

    const auto& ns = manifest.ns;

    // Index commands: "namespace.name" -> handler
    for (const auto& cmd : manifest.commands) {
        std::string fqn = ns + "." + cmd.name;
        command_handlers_[fqn] = cmd.handler;
        std::cerr << "[api] Registered command: " << fqn << std::endl;
    }

    // Index queries: "namespace.name" -> handler
    for (const auto& qry : manifest.queries) {
        std::string fqn = ns + "." + qry.name;
        query_handlers_[fqn] = qry.handler;
        std::cerr << "[api] Registered query: " << fqn << std::endl;
    }

    // Properties auto-generate query/command pairs.
    // getter -> "namespace.get_name", setter -> "namespace.set_name"
    for (const auto& prop : manifest.properties) {
        if (prop.getter) {
            std::string fqn = ns + ".get_" + prop.name;
            query_handlers_[fqn] = prop.getter;
            std::cerr << "[api] Registered property getter: " << fqn << std::endl;
        }
        if (prop.writable && prop.setter) {
            std::string fqn = ns + ".set_" + prop.name;
            command_handlers_[fqn] = prop.setter;
            std::cerr << "[api] Registered property setter: " << fqn << std::endl;
        }
    }

    manifests_.push_back(std::move(manifest));
}

void Dispatcher::register_introspection() {
    // registry.list — returns all registered namespaces and their descriptions.
    query_handlers_["registry.list"] = [this](const Query&) -> QueryResult {
        Args data;
        int i = 0;
        for (const auto& m : manifests_) {
            data[std::to_string(i) + ".ns"] = m.ns;
            data[std::to_string(i) + ".description"] = m.description;

            // Count exports per type
            data[std::to_string(i) + ".commands"] = static_cast<int64_t>(m.commands.size());
            data[std::to_string(i) + ".queries"] = static_cast<int64_t>(m.queries.size());
            data[std::to_string(i) + ".events"] = static_cast<int64_t>(m.events.size());
            data[std::to_string(i) + ".properties"] = static_cast<int64_t>(m.properties.size());
            data[std::to_string(i) + ".references"] = static_cast<int64_t>(m.references.size());
            ++i;
        }
        data["count"] = static_cast<int64_t>(manifests_.size());
        return QueryResult::success(std::move(data));
    };

    // registry.describe — returns full details of a single namespace.
    // Args: { "ns": "buffer" }
    query_handlers_["registry.describe"] = [this](const Query& q) -> QueryResult {
        auto it = q.args.find("ns");
        if (it == q.args.end()) {
            return QueryResult::failure("missing 'ns' arg");
        }
        auto* ns = std::get_if<std::string>(&it->second);
        if (!ns) {
            return QueryResult::failure("'ns' must be a string");
        }

        const ExportManifest* manifest = find_manifest(*ns);
        if (!manifest) {
            return QueryResult::failure("unknown namespace: " + *ns);
        }

        Args data;
        data["ns"] = manifest->ns;
        data["description"] = manifest->description;

        // List commands with their descriptions and arg specs.
        int ci = 0;
        for (const auto& cmd : manifest->commands) {
            std::string prefix = "command." + std::to_string(ci);
            data[prefix + ".name"] = cmd.name;
            data[prefix + ".description"] = cmd.description;
            int ai = 0;
            for (const auto& arg : cmd.args) {
                std::string ap = prefix + ".arg." + std::to_string(ai);
                data[ap + ".name"] = arg.name;
                data[ap + ".type"] = arg.type;
                data[ap + ".required"] = arg.required;
                data[ap + ".description"] = arg.description;
                ++ai;
            }
            data[prefix + ".arg_count"] = static_cast<int64_t>(cmd.args.size());
            ++ci;
        }
        data["command_count"] = static_cast<int64_t>(manifest->commands.size());

        // List queries.
        int qi = 0;
        for (const auto& qry : manifest->queries) {
            std::string prefix = "query." + std::to_string(qi);
            data[prefix + ".name"] = qry.name;
            data[prefix + ".description"] = qry.description;
            ++qi;
        }
        data["query_count"] = static_cast<int64_t>(manifest->queries.size());

        // List events.
        int ei = 0;
        for (const auto& evt : manifest->events) {
            std::string prefix = "event." + std::to_string(ei);
            data[prefix + ".name"] = evt.name;
            data[prefix + ".description"] = evt.description;
            ++ei;
        }
        data["event_count"] = static_cast<int64_t>(manifest->events.size());

        // List properties.
        int pi = 0;
        for (const auto& prop : manifest->properties) {
            std::string prefix = "property." + std::to_string(pi);
            data[prefix + ".name"] = prop.name;
            data[prefix + ".description"] = prop.description;
            data[prefix + ".type"] = prop.type;
            data[prefix + ".writable"] = prop.writable;
            ++pi;
        }
        data["property_count"] = static_cast<int64_t>(manifest->properties.size());

        // List references.
        int ri = 0;
        for (const auto& ref : manifest->references) {
            std::string prefix = "reference." + std::to_string(ri);
            data[prefix + ".name"] = ref.name;
            data[prefix + ".target"] = ref.target_namespace;
            data[prefix + ".description"] = ref.description;
            ++ri;
        }
        data["reference_count"] = static_cast<int64_t>(manifest->references.size());

        return QueryResult::success(std::move(data));
    };

    std::cerr << "[api] Introspection queries registered." << std::endl;
}

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

const ExportManifest* Dispatcher::find_manifest(const std::string& ns) const {
    for (const auto& m : manifests_) {
        if (m.ns == ns) return &m;
    }
    return nullptr;
}

}  // namespace cef_terminal
