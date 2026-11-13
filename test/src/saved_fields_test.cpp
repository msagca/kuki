#include <array>
#include <component_fields.hpp>
#include <cstdint>
#include <engine_archetypes.hpp>
#include <entity_manager.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <saved_fields.hpp>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <world.hpp>
namespace saved_game {
enum class Order : uint8_t { Hold,
  Advance,
  Retreat };
/// A game's component of every kind of field a scene can save, and one it does not.
struct Squad {
  int strength{10};
  int64_t wealth{};
  uint16_t banner{};
  Order order{Order::Hold};
  bool veteran{};
  float morale{1.f};
  double supply{};
  glm::vec2 heading{};
  glm::vec3 rally{};
  glm::vec4 colour{1.f};
  glm::quat facing{1.f, .0f, .0f, .0f};
  kuki::EntityID leader{};
  kuki::AssetID standard{};
  /// Worked out each tick and not saved, which leaving it out of the fields is how to say.
  int cached{-1};
};
/// A game's component that names no fields, which is not saved.
struct Marker {
  int value{};
};
} // namespace saved_game
KUKI_COMPONENT(saved_game::Squad, 0);
KUKI_COMPONENT(saved_game::Marker, 1);
template <>
struct kuki::ComponentFields<saved_game::Squad> {
  static constexpr std::array fields{
    kuki::Field<&saved_game::Squad::strength>("strength"),
    kuki::Field<&saved_game::Squad::wealth>("wealth"),
    kuki::Field<&saved_game::Squad::banner>("banner"),
    kuki::Field<&saved_game::Squad::order>("order"),
    kuki::Field<&saved_game::Squad::veteran>("veteran"),
    kuki::Field<&saved_game::Squad::morale>("morale"),
    kuki::Field<&saved_game::Squad::supply>("supply"),
    kuki::Field<&saved_game::Squad::heading>("heading"),
    kuki::Field<&saved_game::Squad::rally>("rally"),
    kuki::Field<&saved_game::Squad::colour>("colour"),
    kuki::Field<&saved_game::Squad::facing>("facing"),
    kuki::Field<&saved_game::Squad::leader>("leader"),
    kuki::Field<&saved_game::Squad::standard>("standard"),
  };
};
using namespace kuki;
using saved_game::Squad;
namespace {
static_assert(SavedFieldsOf<Squad>.size() == 13);
static_assert(SavedFieldsOf<saved_game::Marker>.empty(), "a component that names no fields saves none");
static_assert(SavedFieldsOf<Squad>[3].kind == FieldKind::Unsigned && SavedFieldsOf<Squad>[3].size == 1, "an enum is saved as its underlying integer");
static_assert(SavedFieldsOf<Squad>[1].kind == FieldKind::Signed && SavedFieldsOf<Squad>[1].size == 8);
static_assert(SavedFieldsOf<Squad>[6].kind == FieldKind::Float && SavedFieldsOf<Squad>[6].size == 8);
using SquadObject = Archetype<Transform, Squad>;
using MarkerObject = Archetype<Transform, saved_game::Marker>;
using SavedWorld = World<SquadObject, MarkerObject>;
auto Filled() -> Squad {
  Squad squad;
  squad.strength = -7;
  squad.wealth = 1LL << 40;
  squad.banner = 60000;
  squad.order = saved_game::Order::Retreat;
  squad.veteran = true;
  squad.morale = .25f;
  squad.supply = 1e10;
  squad.heading = {1.f, -2.f};
  squad.rally = {3.f, 4.f, 5.f};
  squad.colour = {.1f, .2f, .3f, .4f};
  squad.facing = glm::quat{.5f, .5f, .5f, .5f};
  squad.leader = EntityID{42};
  squad.standard = MakeBuiltInAssetID("saved/standard");
  squad.cached = 99;
  return squad;
}
} // namespace
TEST(SavedFields, RoundTripsEveryKindOfField) {
  auto squad = Filled();
  // The leader is the third entity in the file; on load, the third entity made is a new id.
  const std::unordered_map<EntityID, int> order{{EntityID{42}, 2}};
  std::unordered_set<AssetID> referenced;
  const auto json = SavedFieldsToJson(SavedFieldsOf<Squad>, &squad, order, referenced);
  EXPECT_EQ(json.at("strength").get<int>(), -7);
  EXPECT_EQ(json.at("order").get<int>(), 2);
  EXPECT_EQ(json.at("leader").get<int>(), 2) << "an entity is saved as its place in the file";
  EXPECT_FALSE(json.contains("cached")) << "a member not among the fields is not saved";
  EXPECT_TRUE(referenced.contains(squad.standard)) << "an asset field goes in the manifest";
  Squad loaded;
  SavedFieldsFromJson(json, SavedFieldsOf<Squad>, &loaded, {});
  const std::vector created{EntityID{100}, EntityID{101}, EntityID{102}};
  ResolveSavedEntities(SavedFieldsOf<Squad>, &loaded, created);
  EXPECT_EQ(loaded.strength, squad.strength);
  EXPECT_EQ(loaded.wealth, squad.wealth);
  EXPECT_EQ(loaded.banner, squad.banner);
  EXPECT_EQ(loaded.order, squad.order);
  EXPECT_EQ(loaded.veteran, squad.veteran);
  EXPECT_EQ(loaded.morale, squad.morale);
  EXPECT_EQ(loaded.supply, squad.supply);
  EXPECT_EQ(loaded.heading, squad.heading);
  EXPECT_EQ(loaded.rally, squad.rally);
  EXPECT_EQ(loaded.colour, squad.colour);
  EXPECT_EQ(loaded.facing, squad.facing);
  EXPECT_EQ(loaded.leader, EntityID{102});
  EXPECT_EQ(loaded.standard, squad.standard);
  EXPECT_EQ(loaded.cached, -1);
}
TEST(SavedFields, ReadsAFileFromBeforeAFieldWasAddedOrAfterOneWasRemoved) {
  // Saved when the squad had a strength and a field since dropped, and nothing else.
  const auto json = nlohmann::json{{"strength", 3}, {"retired", 12}};
  Squad loaded;
  SavedFieldsFromJson(json, SavedFieldsOf<Squad>, &loaded, {});
  EXPECT_EQ(loaded.strength, 3);
  EXPECT_EQ(loaded.morale, 1.f) << "a field the file has no key for keeps its default";
  EXPECT_EQ(loaded.order, saved_game::Order::Hold);
}
TEST(SavedFields, KeepsTheDefaultOfAFieldOfTheWrongShape) {
  const auto json = nlohmann::json{{"strength", "many"}, {"rally", 4}, {"morale", .5f}};
  Squad loaded;
  SavedFieldsFromJson(json, SavedFieldsOf<Squad>, &loaded, {});
  EXPECT_EQ(loaded.strength, 10);
  EXPECT_EQ(loaded.rally, glm::vec3{});
  EXPECT_EQ(loaded.morale, .5f) << "one bad field does not cost the others";
}
TEST(SavedFields, RemapsAnAssetAndForgetsAnEntityNotInTheFile) {
  auto squad = Filled();
  std::unordered_set<AssetID> referenced;
  const auto json = SavedFieldsToJson(SavedFieldsOf<Squad>, &squad, {}, referenced);
  EXPECT_EQ(json.at("leader").get<int>(), -1);
  const auto moved = MakeBuiltInAssetID("saved/standard, loaded");
  Squad loaded;
  SavedFieldsFromJson(json, SavedFieldsOf<Squad>, &loaded, {{squad.standard, moved}});
  ResolveSavedEntities(SavedFieldsOf<Squad>, &loaded, std::vector{EntityID{0}});
  EXPECT_EQ(loaded.standard, moved);
  EXPECT_EQ(loaded.leader, EntityID::Invalid);
}
TEST(SavedFields, AreFoundThroughAnEntityByTheComponentsId) {
  EntityManager manager;
  for (const auto &spec : SavedWorld::Specs())
    manager.Declare(spec);
  const auto squadId = manager.Create<SquadObject>("Squad");
  const auto markerId = manager.Create<MarkerObject>("Marker");
  manager.GetComponent<Squad>(squadId)->strength = 5;
  const auto saved = manager.GetSavedComponent(squadId, ComponentIdOf<Squad>);
  ASSERT_NE(saved.data, nullptr);
  EXPECT_EQ(saved.fields.size(), SavedFieldsOf<Squad>.size());
  EXPECT_EQ(static_cast<Squad *>(saved.data)->strength, 5);
  const auto marker = manager.GetSavedComponent(markerId, ComponentIdOf<saved_game::Marker>);
  EXPECT_NE(marker.data, nullptr);
  EXPECT_TRUE(marker.fields.empty());
  EXPECT_EQ(manager.GetSavedComponent(markerId, ComponentIdOf<Squad>).data, nullptr) << "an entity without the component has none to save";
}
