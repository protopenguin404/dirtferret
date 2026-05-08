#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// --- Shared value types ---

// Plain dirty rect — used by frame pool, DomBridge, and engine.
// Defined here so low-level modules don't depend on engine.h.
struct DirtyRect {
  int32_t x, y;
  uint32_t width, height;
};

struct Point {
  int32_t x = 0;
  int32_t y = 0;

  bool operator==(const Point &o) const { return x == o.x && y == o.y; }
  bool operator!=(const Point &o) const { return !(*this == o); }
};

struct Region {
  uint32_t id = 0;
  Point anchor; // selection start (same as head for cursor)
  Point head;   // current position (the "active end")

  bool is_cursor() const { return anchor == head; }
};

enum class Scope { Element, Block, Word, Selector };

struct ElementInfo {
  int32_t node_id = -1;
  std::string tag;
  int32_t bounds_x = 0;
  int32_t bounds_y = 0;
  uint32_t bounds_width = 0;
  uint32_t bounds_height = 0;
  std::string text;
  std::vector<std::pair<std::string, std::string>> attributes;
};

// --- RegionSet ---
// Per-buffer collection of Regions. Unified cursor/selection model:
// zero-width Region = cursor, non-zero = selection.
// All operations preserve Region IDs across mutations.

class RegionSet {
public:
  // Add a cursor at point. Returns the region ID.
  uint32_t add(Point p) {
    Region r;
    r.id = next_id_++;
    r.anchor = p;
    r.head = p;
    regions_.push_back(r);
    return r.id;
  }

  // Remove a region by ID. Returns true if found.
  bool remove(uint32_t id) {
    auto it =
        std::find_if(regions_.begin(), regions_.end(),
                      [id](const Region &r) { return r.id == id; });
    if (it == regions_.end())
      return false;
    regions_.erase(it);
    return true;
  }

  // Move a region to point (collapses to cursor: anchor = head = p).
  bool move(uint32_t id, Point p) {
    Region *r = find(id);
    if (!r)
      return false;
    r->anchor = p;
    r->head = p;
    return true;
  }

  // Move all regions by delta.
  void move_all(int32_t dx, int32_t dy) {
    for (auto &r : regions_) {
      r.anchor.x += dx;
      r.anchor.y += dy;
      r.head.x += dx;
      r.head.y += dy;
    }
  }

  // Extend: move head only (anchor stays, creating/growing selection).
  bool extend(uint32_t id, Point p) {
    Region *r = find(id);
    if (!r)
      return false;
    r->head = p;
    return true;
  }

  // Set both anchor and head explicitly.
  bool set_selection(uint32_t id, Point anchor, Point head) {
    Region *r = find(id);
    if (!r)
      return false;
    r->anchor = anchor;
    r->head = head;
    return true;
  }

  // Clear all regions.
  void clear() {
    regions_.clear();
  }

  // Lookup by ID (const).
  const Region *get(uint32_t id) const {
    for (auto &r : regions_) {
      if (r.id == id)
        return &r;
    }
    return nullptr;
  }

  // Primary region (first in list), or nullptr if empty.
  const Region *primary() const {
    return regions_.empty() ? nullptr : &regions_[0];
  }

  const std::vector<Region> &all() const { return regions_; }
  size_t count() const { return regions_.size(); }

private:
  Region *find(uint32_t id) {
    for (auto &r : regions_) {
      if (r.id == id)
        return &r;
    }
    return nullptr;
  }

  std::vector<Region> regions_;
  uint32_t next_id_ = 1;
};
