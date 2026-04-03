#pragma once

#include "api/command.h"
#include "api/exports.h"
#include "api/query.h"
#include "api/types.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace cef_terminal {

// The Dispatcher is the central routing hub of the API layer.
// All cross-layer communication goes through here — no exceptions.
//
// Components register their capabilities via ExportManifest. The dispatcher
// indexes everything by fully-qualified name (namespace.name) and provides
// introspection so any consumer can discover what exists.
class Dispatcher {
 public:
    // Register a component's full export manifest. Indexes all commands,
    // queries, and properties under the manifest's namespace.
    // Properties auto-generate get/set query/command pairs.
    void register_exports(ExportManifest manifest);

    // Legacy single-handler registration (still works, unnamespaced).
    void register_command(const std::string& name, CommandHandler handler);
    void register_query(const std::string& name, QueryHandler handler);

    // Dispatch a command or query by fully-qualified name.
    CommandResult dispatch(const Command& cmd) const;
    QueryResult dispatch(const Query& query) const;

    bool has_command(const std::string& name) const;
    bool has_query(const std::string& name) const;

    // Introspection: get the stored manifests.
    const std::vector<ExportManifest>& manifests() const { return manifests_; }

    // Look up a specific manifest by namespace.
    const ExportManifest* find_manifest(const std::string& ns) const;

 private:
    // Registers built-in introspection queries (registry.*).
    void register_introspection();

    std::unordered_map<std::string, CommandHandler> command_handlers_;
    std::unordered_map<std::string, QueryHandler> query_handlers_;
    std::vector<ExportManifest> manifests_;
    bool introspection_registered_ = false;
};

}  // namespace cef_terminal
