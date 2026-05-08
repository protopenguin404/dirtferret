#include "engine/region.h"
#include <gtest/gtest.h>

TEST(RegionSet, AddReturnsCursor) {
  RegionSet rs;
  auto id = rs.add({10, 20});

  ASSERT_EQ(rs.count(), 1u);
  auto *r = rs.get(id);
  ASSERT_NE(r, nullptr);
  EXPECT_EQ(r->head.x, 10);
  EXPECT_EQ(r->head.y, 20);
  EXPECT_TRUE(r->is_cursor());
}

TEST(RegionSet, AddMultiple) {
  RegionSet rs;
  auto id1 = rs.add({0, 0});
  auto id2 = rs.add({100, 200});

  EXPECT_NE(id1, id2);
  EXPECT_EQ(rs.count(), 2u);
  EXPECT_NE(rs.get(id1), nullptr);
  EXPECT_NE(rs.get(id2), nullptr);
}

TEST(RegionSet, PrimaryIsFirst) {
  RegionSet rs;
  auto id1 = rs.add({1, 2});
  rs.add({3, 4});

  auto *p = rs.primary();
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(p->id, id1);
}

TEST(RegionSet, PrimaryNullWhenEmpty) {
  RegionSet rs;
  EXPECT_EQ(rs.primary(), nullptr);
}

TEST(RegionSet, Remove) {
  RegionSet rs;
  auto id1 = rs.add({0, 0});
  auto id2 = rs.add({1, 1});

  EXPECT_TRUE(rs.remove(id1));
  EXPECT_EQ(rs.count(), 1u);
  EXPECT_EQ(rs.get(id1), nullptr);
  EXPECT_NE(rs.get(id2), nullptr);
}

TEST(RegionSet, RemoveNonexistent) {
  RegionSet rs;
  rs.add({0, 0});
  EXPECT_FALSE(rs.remove(999));
  EXPECT_EQ(rs.count(), 1u);
}

TEST(RegionSet, MoveCollapsesCursor) {
  RegionSet rs;
  auto id = rs.add({0, 0});
  // First make it a selection
  rs.extend(id, {50, 60});
  EXPECT_FALSE(rs.get(id)->is_cursor());

  // Move collapses to cursor
  rs.move(id, {99, 88});
  auto *r = rs.get(id);
  EXPECT_TRUE(r->is_cursor());
  EXPECT_EQ(r->head.x, 99);
  EXPECT_EQ(r->anchor.x, 99);
}

TEST(RegionSet, MoveNonexistent) {
  RegionSet rs;
  EXPECT_FALSE(rs.move(42, {0, 0}));
}

TEST(RegionSet, MoveAll) {
  RegionSet rs;
  rs.add({10, 20});
  rs.add({30, 40});
  rs.move_all(5, -3);

  auto &all = rs.all();
  EXPECT_EQ(all[0].head.x, 15);
  EXPECT_EQ(all[0].head.y, 17);
  EXPECT_EQ(all[1].head.x, 35);
  EXPECT_EQ(all[1].head.y, 37);
}

TEST(RegionSet, ExtendCreatesSelection) {
  RegionSet rs;
  auto id = rs.add({10, 10});
  EXPECT_TRUE(rs.get(id)->is_cursor());

  rs.extend(id, {50, 60});
  auto *r = rs.get(id);
  EXPECT_FALSE(r->is_cursor());
  EXPECT_EQ(r->anchor.x, 10);
  EXPECT_EQ(r->anchor.y, 10);
  EXPECT_EQ(r->head.x, 50);
  EXPECT_EQ(r->head.y, 60);
}

TEST(RegionSet, ExtendNonexistent) {
  RegionSet rs;
  EXPECT_FALSE(rs.extend(42, {0, 0}));
}

TEST(RegionSet, SetSelection) {
  RegionSet rs;
  auto id = rs.add({0, 0});

  rs.set_selection(id, {10, 20}, {30, 40});
  auto *r = rs.get(id);
  EXPECT_EQ(r->anchor.x, 10);
  EXPECT_EQ(r->anchor.y, 20);
  EXPECT_EQ(r->head.x, 30);
  EXPECT_EQ(r->head.y, 40);
  EXPECT_FALSE(r->is_cursor());
}

TEST(RegionSet, SetSelectionNonexistent) {
  RegionSet rs;
  EXPECT_FALSE(rs.set_selection(42, {0, 0}, {1, 1}));
}

TEST(RegionSet, Clear) {
  RegionSet rs;
  rs.add({0, 0});
  rs.add({1, 1});
  rs.add({2, 2});

  rs.clear();
  EXPECT_EQ(rs.count(), 0u);
  EXPECT_EQ(rs.primary(), nullptr);
}

TEST(RegionSet, IdsStableAfterRemoval) {
  RegionSet rs;
  auto id1 = rs.add({0, 0});
  auto id2 = rs.add({1, 1});
  auto id3 = rs.add({2, 2});

  rs.remove(id2);

  // id1 and id3 are unchanged
  EXPECT_NE(rs.get(id1), nullptr);
  EXPECT_EQ(rs.get(id2), nullptr);
  EXPECT_NE(rs.get(id3), nullptr);
  EXPECT_EQ(rs.get(id1)->head.x, 0);
  EXPECT_EQ(rs.get(id3)->head.x, 2);
}

TEST(RegionSet, IdsNeverReused) {
  RegionSet rs;
  auto id1 = rs.add({0, 0});
  rs.remove(id1);
  auto id2 = rs.add({1, 1});

  // New ID must be different from removed one
  EXPECT_NE(id1, id2);
}

TEST(RegionSet, MoveAllAffectsAnchors) {
  RegionSet rs;
  auto id = rs.add({10, 20});
  rs.extend(id, {30, 40});

  rs.move_all(5, 5);

  auto *r = rs.get(id);
  EXPECT_EQ(r->anchor.x, 15);
  EXPECT_EQ(r->anchor.y, 25);
  EXPECT_EQ(r->head.x, 35);
  EXPECT_EQ(r->head.y, 45);
}
