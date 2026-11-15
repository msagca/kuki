#include <game_name.hpp>
#include <gtest/gtest.h>
using kuki::cli::GameName;
using kuki::cli::ParseGameName;
using kuki::cli::RenameTemplate;
TEST(GameName, OneWordIsCapitalisedForTheClass) {
  const auto name = ParseGameName("ddgi");
  ASSERT_TRUE(name);
  EXPECT_EQ(name->directory, "ddgi");
  EXPECT_EQ(name->snake, "ddgi");
  EXPECT_EQ(name->pascal, "Ddgi");
  EXPECT_EQ(name->title, "Ddgi");
}
TEST(GameName, SeparatorsAndHumpsSplitWords) {
  for (const auto *given : {"my-game", "my_game", "MyGame", "myGame"}) {
    const auto name = ParseGameName(given);
    ASSERT_TRUE(name) << given;
    EXPECT_EQ(name->directory, given);
    EXPECT_EQ(name->snake, "my_game") << given;
    EXPECT_EQ(name->pascal, "MyGame") << given;
    EXPECT_EQ(name->title, "My Game") << given;
  }
}
TEST(GameName, CapitalsInAWordAreKept) {
  const auto name = ParseGameName("DDGI");
  ASSERT_TRUE(name);
  EXPECT_EQ(name->snake, "ddgi");
  EXPECT_EQ(name->pascal, "DDGI");
  const auto acronym = ParseGameName("HTTPServer2");
  ASSERT_TRUE(acronym);
  EXPECT_EQ(acronym->snake, "http_server2");
  EXPECT_EQ(acronym->pascal, "HTTPServer2");
}
TEST(GameName, RefusesWhatWouldNotCompileOrNameADirectory) {
  for (const auto *given : {"", "2fast", "-game", "my game", "my.game", "café", "../up", "class", "new", "kuki", "std"})
    EXPECT_FALSE(ParseGameName(given)) << given;
}
TEST(GameName, RenamesEveryPlaceholderInTextAndPaths) {
  const auto name = ParseGameName("ddgi").value();
  EXPECT_EQ(RenameTemplate("class MyGame : SystemApplication<my_game::MyGameWorld>", name), "class Ddgi : SystemApplication<ddgi::DdgiWorld>");
  EXPECT_EQ(RenameTemplate("SetupMyGame(app); \"My Game\"; [MyGame]", name), "SetupDdgi(app); \"Ddgi\"; [Ddgi]");
  EXPECT_EQ(RenameTemplate("game/include/my_game_world.hpp", name), "game/include/ddgi_world.hpp");
  EXPECT_EQ(RenameTemplate("nothing to rename", name), "nothing to rename");
}
