#include <gtest/gtest.h>
#include <set>
#include <stress_system.hpp>
#include <string_view>
using kuki::FindStressPreset;
using kuki::STRESS_PRESETS;
using kuki::StressLoad;
using kuki::StressSystem;
TEST(Stress, FindsEveryPresetByItsNameAndNoOther) {
  std::set<std::string_view> names;
  for (const auto &preset : STRESS_PRESETS) {
    EXPECT_TRUE(names.insert(preset.name).second) << preset.name << " is named twice";
    const auto found = FindStressPreset(preset.name);
    ASSERT_TRUE(found) << preset.name;
    EXPECT_EQ(*found, preset.load);
  }
  EXPECT_FALSE(FindStressPreset("everything"));
  EXPECT_FALSE(FindStressPreset(""));
}
// `all` is what `--stress all` and the editor's panel start from, and is meant to be every other
// load at once; one changed without the other would make `all` quietly test something else.
TEST(Stress, AllIsEveryOtherLoadAtOnce) {
  const auto all = FindStressPreset("all");
  ASSERT_TRUE(all);
  StressLoad sum{.pools = 0};
  for (const auto &preset : STRESS_PRESETS) {
    if (preset.name == "all")
      continue;
    sum.instances += preset.load.instances;
    sum.rigs += preset.load.rigs;
    sum.lights += preset.load.lights;
    sum.emitters += preset.load.emitters;
    if (preset.load.instances > 0)
      sum.pools = preset.load.pools;
  }
  EXPECT_EQ(all->instances, sum.instances);
  EXPECT_EQ(all->pools, sum.pools);
  EXPECT_EQ(all->rigs, sum.rigs);
  EXPECT_EQ(all->lights, sum.lights);
  EXPECT_EQ(all->emitters, sum.emitters);
}
TEST(Stress, PresetsAskForNoMoreThanCanBeMade) {
  for (const auto &preset : STRESS_PRESETS) {
    EXPECT_GE(preset.load.pools, 1u) << preset.name;
    EXPECT_LE(preset.load.pools, StressSystem::MAX_POOLS) << preset.name;
    // Eight point lights and eight spot lights are what the renderer shades; more measure nothing.
    EXPECT_LE(preset.load.lights, 16u) << preset.name;
  }
}
