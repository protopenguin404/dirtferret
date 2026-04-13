#include "engine/engine.h"
#include "engine/region.h"
#include <gtest/gtest.h>

// Tests for find_nearest_in_direction — the spatial navigation algorithm.
// This is a free function declared in engine.h and defined in engine.cc,
// testable without CEF or DomBridge.

// Helper to make an ElementInfo at a given center point with default size.
static ElementInfo make_element(int32_t node_id, int cx, int cy,
                                uint32_t w = 100, uint32_t h = 30) {
  ElementInfo info;
  info.node_id = node_id;
  info.bounds_x = cx - (int)(w / 2);
  info.bounds_y = cy - (int)(h / 2);
  info.bounds_width = w;
  info.bounds_height = h;
  info.tag = "A";
  return info;
}

// --- Basic directional tests ---

TEST(FindNearestInDirection, MoveRightPicksRightwardNeighbor) {
  //  [0]   [1]   [2]
  //  100   300   500   (center x)
  //  all at y=100
  std::vector<ElementInfo> elements = {
      make_element(0, 100, 100),
      make_element(1, 300, 100),
      make_element(2, 500, 100),
  };

  // From element 0, move right (dx=1, dy=0)
  int result = find_nearest_in_direction(elements, 0, 1, 0);
  EXPECT_EQ(result, 1);

  // From element 1, move right
  result = find_nearest_in_direction(elements, 1, 1, 0);
  EXPECT_EQ(result, 2);
}

TEST(FindNearestInDirection, MoveLeftPicksLeftwardNeighbor) {
  std::vector<ElementInfo> elements = {
      make_element(0, 100, 100),
      make_element(1, 300, 100),
      make_element(2, 500, 100),
  };

  // From element 2, move left (dx=-1, dy=0)
  int result = find_nearest_in_direction(elements, 2, -1, 0);
  EXPECT_EQ(result, 1);

  // From element 1, move left
  result = find_nearest_in_direction(elements, 1, -1, 0);
  EXPECT_EQ(result, 0);
}

TEST(FindNearestInDirection, MoveDownPicksDownwardNeighbor) {
  //  [0] at (200, 100)
  //  [1] at (200, 300)
  //  [2] at (200, 500)
  std::vector<ElementInfo> elements = {
      make_element(0, 200, 100),
      make_element(1, 200, 300),
      make_element(2, 200, 500),
  };

  // From element 0, move down (dx=0, dy=1)
  int result = find_nearest_in_direction(elements, 0, 0, 1);
  EXPECT_EQ(result, 1);

  // From element 1, move down
  result = find_nearest_in_direction(elements, 1, 0, 1);
  EXPECT_EQ(result, 2);
}

TEST(FindNearestInDirection, MoveUpPicksUpwardNeighbor) {
  std::vector<ElementInfo> elements = {
      make_element(0, 200, 100),
      make_element(1, 200, 300),
      make_element(2, 200, 500),
  };

  // From element 2, move up (dx=0, dy=-1)
  int result = find_nearest_in_direction(elements, 2, 0, -1);
  EXPECT_EQ(result, 1);
}

// --- Edge cases ---

TEST(FindNearestInDirection, NoElementInDirectionReturnsNegativeOne) {
  std::vector<ElementInfo> elements = {
      make_element(0, 100, 100),
      make_element(1, 300, 100),
  };

  // From element 1 (rightmost), move right — nothing there
  int result = find_nearest_in_direction(elements, 1, 1, 0);
  EXPECT_EQ(result, -1);

  // From element 0 (leftmost), move left — nothing there
  result = find_nearest_in_direction(elements, 0, -1, 0);
  EXPECT_EQ(result, -1);
}

TEST(FindNearestInDirection, SingleElementReturnsNegativeOne) {
  std::vector<ElementInfo> elements = {
      make_element(0, 200, 200),
  };

  EXPECT_EQ(find_nearest_in_direction(elements, 0, 1, 0), -1);
  EXPECT_EQ(find_nearest_in_direction(elements, 0, -1, 0), -1);
  EXPECT_EQ(find_nearest_in_direction(elements, 0, 0, 1), -1);
  EXPECT_EQ(find_nearest_in_direction(elements, 0, 0, -1), -1);
}

TEST(FindNearestInDirection, EmptyElementsReturnsNegativeOne) {
  std::vector<ElementInfo> elements;
  EXPECT_EQ(find_nearest_in_direction(elements, 0, 1, 0), -1);
}

TEST(FindNearestInDirection, InvalidCurrentIndexReturnsNegativeOne) {
  std::vector<ElementInfo> elements = {
      make_element(0, 100, 100),
  };
  EXPECT_EQ(find_nearest_in_direction(elements, -1, 1, 0), -1);
  EXPECT_EQ(find_nearest_in_direction(elements, 5, 1, 0), -1);
}

// --- Grid layout ---

TEST(FindNearestInDirection, GridNavigationPicksNearest) {
  // 2x2 grid:
  //  [0](100,100)  [1](300,100)
  //  [2](100,300)  [3](300,300)
  std::vector<ElementInfo> elements = {
      make_element(0, 100, 100),
      make_element(1, 300, 100),
      make_element(2, 100, 300),
      make_element(3, 300, 300),
  };

  // From top-left, move right -> top-right
  EXPECT_EQ(find_nearest_in_direction(elements, 0, 1, 0), 1);

  // From top-left, move down -> bottom-left
  EXPECT_EQ(find_nearest_in_direction(elements, 0, 0, 1), 2);

  // From bottom-right, move left -> bottom-left
  EXPECT_EQ(find_nearest_in_direction(elements, 3, -1, 0), 2);

  // From bottom-right, move up -> top-right
  EXPECT_EQ(find_nearest_in_direction(elements, 3, 0, -1), 1);
}

TEST(FindNearestInDirection, GridDiagonalFiltersCorrectly) {
  // 2x2 grid — moving right from top-left should NOT pick bottom-left
  // (it's below, not to the right)
  std::vector<ElementInfo> elements = {
      make_element(0, 100, 100),
      make_element(1, 300, 100),
      make_element(2, 100, 300),
      make_element(3, 300, 300),
  };

  // From top-left, move right. Both [1](300,100) and [3](300,300) are
  // in the right half-plane. [1] is closer (200 vs ~283).
  EXPECT_EQ(find_nearest_in_direction(elements, 0, 1, 0), 1);
}

TEST(FindNearestInDirection, PicksClosestWhenMultipleInDirection) {
  // Three elements in a line going right, pick the nearest
  std::vector<ElementInfo> elements = {
      make_element(0, 100, 100),
      make_element(1, 200, 100),
      make_element(2, 500, 100),
  };

  // From 0, move right — 1 is closer than 2
  EXPECT_EQ(find_nearest_in_direction(elements, 0, 1, 0), 1);
}

TEST(FindNearestInDirection, PerpendicularElementsFiltered) {
  // Element directly above and directly to the right.
  // Moving right should not pick the one above.
  std::vector<ElementInfo> elements = {
      make_element(0, 200, 200),  // current
      make_element(1, 200, 50),   // directly above
      make_element(2, 400, 200),  // directly right
  };

  // Move right: only element 2 is in the right half-plane
  // (element 1 has delta_x=0, dot = 0*1 + (-150)*0 = 0, filtered out)
  EXPECT_EQ(find_nearest_in_direction(elements, 0, 1, 0), 2);

  // Move up: only element 1 is in the up half-plane
  EXPECT_EQ(find_nearest_in_direction(elements, 0, 0, -1), 1);
}
