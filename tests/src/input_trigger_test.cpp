#include <GLFW/glfw3.h>
#include <gtest/gtest.h>
#include <input_manager.hpp>
using kuki::FormatTrigger;
using kuki::InputManager;
using kuki::ParseTrigger;
TEST(InputTrigger, ComesBackAsItWasSaved) {
  for (const auto trigger : {InputManager::Trigger{GLFW_KEY_J, 0}, InputManager::Trigger{GLFW_KEY_C, GLFW_MOD_CONTROL}, InputManager::Trigger{GLFW_KEY_LEFT_SHIFT, 0}, InputManager::Trigger{GLFW_KEY_F5, GLFW_MOD_SHIFT | GLFW_MOD_ALT}}) {
    const auto parsed = ParseTrigger(FormatTrigger(trigger));
    ASSERT_TRUE(parsed) << FormatTrigger(trigger);
    EXPECT_EQ(*parsed, trigger);
  }
}
// Unbound is a choice worth keeping: a key taken by another binding leaves this one with none, and
// loading its default instead would take the key back.
TEST(InputTrigger, KeepsUnbound) {
  const auto parsed = ParseTrigger(FormatTrigger({}));
  ASSERT_TRUE(parsed);
  EXPECT_EQ(parsed->key, 0);
}
TEST(InputTrigger, RefusesWhatItDidNotWrite) {
  for (const auto *text : {"", "74", "74 ", " 74 0", "74 0 1", "J 0", "74 x", "-1 0", "74 -2"})
    EXPECT_FALSE(ParseTrigger(text)) << '"' << text << '"';
}
