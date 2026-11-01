#include <gtest/gtest.h>
#include <id.hpp>
#include <slot_store.hpp>
#include <vector>
using namespace kuki;
namespace {
using Store = SlotStore<std::vector<int>>;
} // namespace
TEST(SlotStore, ClaimsOnceAndFindsItAgain) {
  Store store;
  SlotHandle handle;
  const EntityID owner{3};
  store.Claim(handle, owner).push_back(7);
  ASSERT_NE(handle.index, SlotHandle::NONE);
  const auto before = handle.index;
  store.Claim(handle, owner).push_back(8);
  EXPECT_EQ(handle.index, before);
  ASSERT_NE(store.Find(handle, owner), nullptr);
  EXPECT_EQ(store.Find(handle, owner)->size(), 2u);
  EXPECT_EQ(store.Count(), 1u);
}
TEST(SlotStore, AnswersOnlyToTheEntityThatClaimed) {
  Store store;
  SlotHandle handle;
  store.Claim(handle, EntityID{3}).push_back(7);
  // A component copied to another entity carries the handle; the slot is not that entity's.
  auto copied = handle;
  EXPECT_EQ(store.Find(copied, EntityID{4}), nullptr);
  EXPECT_EQ(store.Find(copied, EntityID::Invalid), nullptr);
  store.Claim(copied, EntityID{4});
  EXPECT_NE(copied.index, handle.index);
  EXPECT_TRUE(store.Find(copied, EntityID{4})->empty());
  EXPECT_EQ(store.Find(handle, EntityID{3})->size(), 1u);
}
TEST(SlotStore, SweepsTheSlotsOfEntitiesThatHaveGoneAndReusesThem) {
  Store store;
  SlotHandle kept;
  SlotHandle gone;
  store.Claim(kept, EntityID{1}).push_back(1);
  store.Claim(gone, EntityID{2}).push_back(2);
  store.Sweep([](const EntityID id) { return id == EntityID{1}; });
  EXPECT_EQ(store.Count(), 1u);
  EXPECT_NE(store.Find(kept, EntityID{1}), nullptr);
  EXPECT_EQ(store.Find(gone, EntityID{2}), nullptr);
  // The emptied slot is the next one given out, and holds nothing of what it held.
  SlotHandle next;
  EXPECT_TRUE(store.Claim(next, EntityID{5}).empty());
  EXPECT_EQ(next.index, gone.index);
}
TEST(SlotStore, AHandleLeadingNowhereFindsNothing) {
  const Store store;
  EXPECT_EQ(store.Find(SlotHandle{}, EntityID{1}), nullptr);
  EXPECT_EQ(store.Find(SlotHandle{9}, EntityID{1}), nullptr);
}
