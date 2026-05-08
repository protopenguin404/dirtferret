// Standalone compilation of find_nearest_in_direction for unit testing.
// This duplicates the implementation from engine.cc so the test binary
// does not need to link against CEF.

#include "engine/region.h"

#include <cmath>
#include <cstdint>
#include <vector>

int find_nearest_in_direction(const std::vector<ElementInfo>& elements,
                              int current_index, int dx, int dy) {
  if (current_index < 0 || current_index >= (int)elements.size()) return -1;

  auto& current = elements[current_index];
  int cx = current.bounds_x + (int)current.bounds_width / 2;
  int cy = current.bounds_y + (int)current.bounds_height / 2;

  int best_index = -1;
  double best_dist = 1e18;

  for (size_t i = 0; i < elements.size(); ++i) {
    if ((int)i == current_index) continue;
    auto& cand = elements[i];
    int ex = cand.bounds_x + (int)cand.bounds_width / 2;
    int ey = cand.bounds_y + (int)cand.bounds_height / 2;

    int delta_x = ex - cx;
    int delta_y = ey - cy;

    // Check forward half-plane: dot product with direction > 0
    int dot = delta_x * dx + delta_y * dy;
    if (dot <= 0) continue;

    double dist = std::sqrt((double)(delta_x * delta_x + delta_y * delta_y));
    if (dist < best_dist) {
      best_dist = dist;
      best_index = (int)i;
    }
  }

  return best_index;
}
