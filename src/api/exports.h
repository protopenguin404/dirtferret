#pragma once

#include "api/command.h"
#include "api/query.h"
#include "api/types.h"

#include <string>
#include <vector>

namespace cef_terminal {

// Describes one argument a command/query expects or an event carries.
struct ArgSpec {
    std::string name;
    std::string type;           // "string", "int", "bool", "double"
    bool required = true;
    std::string description;
};

// A declared command: fire-and-forget action that may modify state.
struct CommandExport {
    std::string name;           // local name, e.g., "navigate" (namespaced by manifest)
    std::string description;
    std::vector<ArgSpec> args;
    CommandHandler handler;
};

// A declared query: read-only request that returns data.
struct QueryExport {
    std::string name;
    std::string description;
    std::vector<ArgSpec> args;
    std::vector<ArgSpec> returns;
    QueryHandler handler;
};

// A declared event: something the component emits. No handler — subscribers
// register interest through the dispatcher's event system (future).
struct EventExport {
    std::string name;
    std::string description;
    std::vector<ArgSpec> payload;
};

// A declared property: observable state with get (and optionally set).
// Collapses a query+command pair into a single declared thing.
struct PropertyExport {
    std::string name;
    std::string description;
    std::string type;           // "string", "int", "bool", "double"
    bool writable = false;
    QueryHandler getter;        // always required
    CommandHandler setter;      // only if writable
};

// A reference to another export namespace. Declares a dependency/relationship.
struct ReferenceExport {
    std::string name;               // local name for this reference
    std::string target_namespace;   // the namespace it points to
    std::string description;
};

// The complete set of runtime exports declared by one component.
// The namespace is the component's identity in the system.
struct ExportManifest {
    std::string ns;             // namespace, e.g., "buffer"
    std::string description;    // what this component does

    std::vector<CommandExport> commands;
    std::vector<QueryExport> queries;
    std::vector<EventExport> events;
    std::vector<PropertyExport> properties;
    std::vector<ReferenceExport> references;
};

}  // namespace cef_terminal
