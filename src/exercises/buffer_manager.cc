// ============================================================================
// EXERCISE: BufferManager — Memory Management & OO Patterns
// ============================================================================
//
// This is a standalone learning exercise. It does NOT link against CEF.
// Compile with: g++ -std=c++17 -o buffer_manager buffer_manager.cc
// (or add it to CMakeLists.txt yourself — that's part of the exercise)
//
// ---- TASK DESCRIPTION ----
//
// Build a BufferManager class.
//
// Requirements:
//
// - BufferManager owns a collection of Buffer objects (you'll define Buffer
//   as a simple struct/class — no CEF, just mock data: an id, url, title,
//   state enum)
//
// - Buffers are created via a factory method that returns the buffer's ID.
//   Internally stored as std::unique_ptr<Buffer> — the manager owns them
//   exclusively.
//
// - Support get_buffer(id) returning a non-owning raw pointer (or nullptr
//   if not found).
//
// - Support remove_buffer(id) that destroys the buffer.
//
// - Add an Observer interface (pure virtual class) that gets notified on
//   buffer creation and destruction. Multiple observers can be registered.
//   Observers hold std::shared_ptr / std::weak_ptr to manage their own
//   lifetimes — if an observer is destroyed, the manager shouldn't crash
//   calling it.
//
// - Write a small main() that exercises: creating buffers, querying them,
//   attaching/detaching observers, removing buffers, and demonstrating
//   that a dangling observer reference is safely handled.
//
// ---- WHAT THIS EXERCISES ----
//
// - std::unique_ptr: exclusive ownership (manager owns buffers, nobody else)
// - std::shared_ptr / std::weak_ptr: shared vs non-owning references
//   (observer lifetime)
// - Raw pointers as non-owning views: the get_buffer() return
// - Virtual destructors and pure virtual interfaces: the Observer pattern
// - Potential memory bugs: what happens if you forget virtual destructors,
//   dereference after remove, or hold a stale observer
//
// ============================================================================

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

int main() {
    std::cout << "Hello from buffer_manager exercise!" << std::endl;
    std::cout << "Replace this with your implementation." << std::endl;
    return 0;
}
