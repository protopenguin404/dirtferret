// ============================================================================
// EXERCISE: Runtime Exports — Declarative Component Surfaces
// ============================================================================
//
// This is a standalone learning exercise. It does NOT link against CEF.
// Compile with: g++ -std=c++17 -o runtime_exports runtime_exports.cc
// (or add it to CMakeLists.txt yourself — that's part of the exercise)
//
// ---- BACKGROUND ----
//
// In cef-terminal, every major component declares what it offers to the
// rest of the system as a structured "export manifest." This is like a
// super-public access modifier: the component decides exactly what is
// visible to other layers, plugins, and IPC consumers.
//
// The dispatcher collects these manifests, indexes the handlers by
// fully-qualified name (namespace.name), and provides introspection
// so anything can discover what exists at runtime.
//
// This exercise rebuilds a simplified version of that system from scratch
// so you understand how the pieces fit together.
//
// ---- TASK DESCRIPTION ----
//
// Build a mini runtime export system with these parts:
//
// 1. VALUE TYPES
//    Define a Value type (std::variant<std::monostate, bool, int, double,
//    std::string>) and an Args type (std::unordered_map<std::string, Value>).
//
// 2. RESULTS
//    Define CommandResult { bool ok; std::string error; } with static
//    success() and failure(msg) factories.
//    Define QueryResult { bool ok; std::string error; Args data; } similarly.
//
// 3. EXPORT TYPES
//    Define these structs:
//    - ArgSpec { name, type_name, required, description }
//    - CommandExport { name, description, vector<ArgSpec> args, handler }
//    - QueryExport { name, description, vector<ArgSpec> args, handler }
//    - PropertyExport { name, description, type_name, writable, getter, setter }
//    - ExportManifest { ns, description, vector of each export type }
//
//    Handlers are std::function:
//      CommandHandler = std::function<CommandResult(const Args&)>
//      QueryHandler = std::function<QueryResult(const Args&)>
//
// 4. REGISTRY
//    Build a Registry class that:
//    - Accepts ExportManifest via register_exports(manifest)
//    - Indexes command handlers as "namespace.name"
//    - Indexes query handlers as "namespace.name"
//    - Auto-generates "namespace.get_prop" queries from property getters
//    - Auto-generates "namespace.set_prop" commands from writable property setters
//    - dispatch_command(name, args) -> CommandResult
//    - dispatch_query(name, args) -> QueryResult
//    - list_namespaces() -> vector<string>
//    - describe(namespace) -> prints or returns info about all exports
//
// 5. SAMPLE COMPONENTS
//    Create two fake components that each produce an ExportManifest:
//
//    a) "counter" namespace:
//       - Command "increment" (arg: "amount", type int, default 1)
//       - Command "reset"
//       - Query "get_value" (returns {"value": current_count})
//       - Property "value" (read-only, returns current count)
//       The counter state is just an int captured by the lambdas.
//
//    b) "greeter" namespace:
//       - Command "set_greeting" (arg: "text", type string)
//       - Query "greet" (arg: "name", type string; returns {"message": "Hello, name!"})
//       - Property "greeting" (writable, get/set the greeting template)
//
// 6. MAIN
//    In main():
//    - Create both components' manifests and register them
//    - Dispatch some commands and queries, printing results
//    - List all namespaces
//    - Describe each namespace (print all exports with their arg specs)
//    - Demonstrate that counter.get_value and counter.increment work
//    - Demonstrate that properties work (counter.get_value via property,
//      greeter.set_greeting + greeter.get_greeting)
//    - Try dispatching an unknown command and show the error
//
// ---- WHAT THIS EXERCISES ----
//
// - std::variant and std::visit for tagged union types
// - std::function and lambda captures (mutable lambdas for state)
// - std::unordered_map for registries
// - The pattern of declarative manifests vs. imperative registration
// - How introspection falls out naturally from structured declarations
// - How auto-generated property getters/setters reduce boilerplate
// - Namespacing as an emergent property of component identity
//
// ---- HINTS ----
//
// - Your lambdas for the counter will need to capture a shared_ptr<int>
//   (or a raw pointer to a local) so multiple handlers can share the
//   same counter state. A mutable lambda capturing by value won't work
//   because each handler gets its own copy.
//
// - For the property auto-generation, the registry just needs to check
//   if a PropertyExport has a getter and create a query handler, and
//   if it's writable with a setter, create a command handler.
//
// - To print a Value, you can use std::visit with a visitor lambda:
//     std::visit([](auto&& v) {
//         using T = std::decay_t<decltype(v)>;
//         if constexpr (std::is_same_v<T, std::monostate>) std::cout << "null";
//         else std::cout << v;
//     }, value);
//
// - Look at src/api/exports.h and src/api/dispatcher.cc for the real
//   implementation. This exercise is a simplified version of that code.
//
// ============================================================================

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <variant>

int main() {
    std::cout << "Hello from runtime_exports exercise!" << std::endl;
    std::cout << "Replace this with your implementation." << std::endl;
    return 0;
}
