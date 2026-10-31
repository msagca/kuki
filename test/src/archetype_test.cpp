#include <command_line.hpp>
#include <component.hpp>
#include <engine_archetypes.hpp>
#include <entity_manager.hpp>
#include <glm/ext/vector_float3.hpp>
#include <gtest/gtest.h>
#include <network_options.hpp>
#include <string>
#include <unordered_set>
#include <vector>
#include <world.hpp>
namespace test_game {
struct Health {
  int points{100};
};
struct Tag {};
/// Given the index `Health` has, which nothing stops until a world holds both.
struct Clash {};
} // namespace test_game
KUKI_COMPONENT(test_game::Health, 0);
KUKI_COMPONENT(test_game::Tag, 1);
KUKI_COMPONENT(test_game::Clash, 0);
using namespace kuki;
using test_game::Health;
using test_game::Tag;
namespace {
using Unit = Archetype<Transform, Health>;
using TaggedUnit = Archetype<Transform, Health, Tag>;
using Marker = Archetype<Tag>;
using TestWorld = World<Unit, TaggedUnit, Marker>;
/// @brief Tells a manager about every archetype of the test's world, as a scene is told of its game's.
auto Declare(EntityManager &manager) -> void {
  for (const auto &spec : TestWorld::Specs())
    manager.Declare(spec);
}
// What the compiler is asked to know, which the rest of the engine leans on. A test of these that
// failed would fail to build, so they are here and not in a TEST.
static_assert(ComponentIdOf<Transform> == static_cast<ComponentId>(ComponentType::Transform));
static_assert(ComponentIdOf<Health> == FIRST_GAME_COMPONENT);
static_assert(Unit::Has<Health> && !Unit::Has<Tag>);
static_assert(Archetype<Transform, Renderable>::Has<Pooled>, "a bundle in an archetype is its components");
static_assert(Archetype<Transform>::Has<WorldMatrix>, "a transform brings its world matrix");
static_assert(Archetype<Transform, Health>::bits == Archetype<Health, Transform>::bits, "the order of an archetype's components does not matter");
static_assert(TestWorld::Has<Unit> && TestWorld::Has<MeshObject> && !TestWorld::Has<Archetype<Health>>);
static_assert(TestWorld::Matches<Health> && TestWorld::Matches<Transform, Without<Tag>> && !TestWorld::Matches<Health, Camera>);
static_assert(TestWorld::Uses<Tag> && !TestWorld::Uses<TextureHandle>);
static_assert(IsQuery<Transform, Optional<Tag>> && !IsQuery<Optional<Tag>> && !IsQuery<Tag, Without<Tag>>);
static_assert(QueryMatches<Health, Without<Tag>>(Unit::bits) && !QueryMatches<Health, Without<Tag>>(TaggedUnit::bits));
static_assert(!detail::DistinctSignatures<TypeList<Unit, Archetype<Health, Transform>>>, "a world refuses two archetypes of the same components");
static_assert(!HasDistinctIds<TypeList<Health, Tag, test_game::Clash>>, "a world refuses two components given the same index");
} // namespace
TEST(Archetype, MakesAnEntityOfADeclaredArchetypeAndNoOther) {
  EntityManager manager;
  Declare(manager);
  const auto unit = manager.Create<Unit>("unit");
  ASSERT_TRUE(unit);
  EXPECT_EQ(manager.GetSignature(unit), Unit::Mask());
  EXPECT_NE(manager.GetComponent<Health>(unit), nullptr);
  EXPECT_EQ(manager.GetComponent<Tag>(unit), nullptr);
  EXPECT_EQ(manager.GetComponent<Health>(unit)->points, 100) << "a component starts at its defaults";
  EXPECT_TRUE(manager.HasComponent<Transform>(unit));
  EXPECT_FALSE((manager.HasComponent<Transform, Tag>(unit)));
  // Not declared: no entity, rather than an archetype made on the spot.
  EXPECT_FALSE(manager.Create<Archetype<Health>>("stray"));
  EXPECT_EQ(manager.GetCount(), 1u);
}
TEST(Archetype, QueriesVisitTheArchetypesTheyMatch) {
  EntityManager manager;
  Declare(manager);
  const auto plain = manager.Create<Unit>();
  const auto tagged = manager.Create<TaggedUnit>();
  const auto marker = manager.Create<Marker>();
  std::unordered_set<EntityID> health, untagged, optional;
  manager.ForEach<Health>([&](const EntityID id, Health *) { health.insert(id); });
  manager.ForEach<Health, Without<Tag>>([&](const EntityID id, Health *) { untagged.insert(id); });
  auto withTag = 0;
  manager.ForEach<Transform, Optional<Tag>>([&](const EntityID id, Transform *, Tag *tag) {
    optional.insert(id);
    withTag += tag != nullptr;
  });
  EXPECT_EQ(health, (std::unordered_set{plain, tagged}));
  EXPECT_EQ(untagged, (std::unordered_set{plain}));
  EXPECT_EQ(optional, (std::unordered_set{plain, tagged}));
  EXPECT_EQ(withTag, 1);
  std::unordered_set<EntityID> markers;
  manager.ForEach<Tag, Without<Transform>>([&](const EntityID id, Tag *) { markers.insert(id); });
  EXPECT_EQ(markers, (std::unordered_set{marker}));
}
TEST(Archetype, AQueryOfOneArchetypeFindsWhatTheWideQueryFindsThere) {
  EntityManager manager;
  Declare(manager);
  for (auto i = 0; i < 5; ++i)
    manager.GetComponent<Health>(manager.Create<Unit>())->points = i;
  manager.Create<TaggedUnit>();
  std::vector<int> wide, narrow;
  manager.ForEach<Health, Optional<Tag>, Without<Tag>>([&](const EntityID, Health *h, Tag *tag) {
    EXPECT_EQ(tag, nullptr);
    wide.push_back(h->points);
  });
  manager.ForEachOf<Unit, Health, Optional<Tag>, Without<Tag>>([&](const EntityID, Health *h, Tag *tag) {
    EXPECT_EQ(tag, nullptr) << "an optional component the archetype lacks is handed over as null";
    narrow.push_back(h->points);
  });
  EXPECT_EQ(wide, narrow);
  EXPECT_EQ(narrow, (std::vector{0, 1, 2, 3, 4}));
}
TEST(Archetype, DeletingMovesTheLastRowIntoTheGapAndKeepsEveryoneFindable) {
  EntityManager manager;
  Declare(manager);
  std::vector<EntityID> units;
  for (auto i = 0; i < 4; ++i) {
    units.push_back(manager.Create<Unit>());
    manager.GetComponent<Health>(units.back())->points = i;
  }
  ASSERT_TRUE(manager.Delete(units[1]));
  EXPECT_FALSE(manager.IsEntity(units[1]));
  EXPECT_EQ(manager.GetComponent<Health>(units[0])->points, 0);
  EXPECT_EQ(manager.GetComponent<Health>(units[2])->points, 2);
  EXPECT_EQ(manager.GetComponent<Health>(units[3])->points, 3) << "the row that was moved into the gap is still its own entity's";
}
TEST(Archetype, ADeleteDuringAQueryWaitsForItToFinish) {
  EntityManager manager;
  Declare(manager);
  for (auto i = 0; i < 3; ++i)
    manager.Create<Unit>();
  auto visited = 0;
  manager.ForEach<Health>([&](const EntityID id, Health *) {
    ++visited;
    manager.Delete(id);
  });
  EXPECT_EQ(visited, 3) << "every entity is visited once, the deletes notwithstanding";
  EXPECT_EQ(manager.GetCount(), 0u);
}
TEST(Archetype, ChildrenNeedTransformsAndMoveWithTheirParent) {
  EntityManager manager;
  Declare(manager);
  const auto parent = manager.Create<Unit>("parent");
  const auto child = manager.Create<TaggedUnit>("child");
  const auto marker = manager.Create<Marker>("marker");
  EXPECT_FALSE(manager.AddChild(parent, marker)) << "an entity with no transform has nothing to inherit";
  ASSERT_TRUE(manager.AddChild(parent, child));
  manager.GetComponent<Transform>(child)->position = {1.f, .0f, .0f};
  manager.GetComponent<Transform>(parent)->position = {.0f, 2.f, .0f};
  manager.UpdateComponents<Transform>();
  EXPECT_FLOAT_EQ(manager.GetComponent<WorldMatrix>(child)->matrix[3].y, 2.f);
  EXPECT_FLOAT_EQ(manager.GetComponent<WorldMatrix>(child)->matrix[3].x, 1.f);
  // A row added to the parent's archetype can move its column, which the transform pass holds
  // pointers into. It has to notice and not write through a stale one.
  for (auto i = 0; i < 64; ++i)
    manager.Create<Unit>();
  manager.GetComponent<Transform>(parent)->position = {.0f, 5.f, .0f};
  manager.MarkTransformDirty(parent);
  manager.UpdateComponents<Transform>();
  EXPECT_FLOAT_EQ(manager.GetComponent<WorldMatrix>(child)->matrix[3].y, 5.f);
}
TEST(Archetype, CopiesAnEntityAndItsChildrenToAnotherManager) {
  EntityManager from;
  Declare(from);
  EntityManager to;
  Declare(to);
  const auto parent = from.Create<Unit>("parent");
  const auto child = from.Create<TaggedUnit>("child");
  from.AddChild(parent, child);
  from.GetComponent<Health>(parent)->points = 7;
  from.GetComponent<Health>(child)->points = 9;
  const auto copy = to.CopyFrom(from, parent);
  ASSERT_TRUE(copy);
  EXPECT_EQ(to.GetComponent<Health>(copy)->points, 7);
  ASSERT_TRUE(to.HasChildren(copy));
  to.ForEachChild(copy, [&](const EntityID id) {
    EXPECT_EQ(to.GetSignature(id), TaggedUnit::Mask());
    EXPECT_EQ(to.GetComponent<Health>(id)->points, 9);
    EXPECT_EQ(to.GetParent(id), copy);
  });
  // Into an entity of another archetype, what they share and no more, and what was left is said.
  const auto marker = to.Create<Marker>();
  const auto left = to.CopyComponents(from, child, marker);
  EXPECT_EQ(left, (MaskOf<Transform, WorldMatrix, Health>()));
  // A manager that was not told of the archetype makes no copy.
  EntityManager bare;
  EXPECT_FALSE(bare.CopyFrom(from, parent));
}
TEST(Archetype, ClearingKeepsTheDeclarations) {
  EntityManager manager;
  Declare(manager);
  manager.Create<Unit>();
  manager.Clear();
  EXPECT_EQ(manager.GetCount(), 0u);
  EXPECT_TRUE(manager.Create<Unit>());
}
TEST(Archetype, RunTimeQuestionsAboutComponentsAreGeneratedFromTheList) {
  EntityManager manager;
  Declare(manager);
  const auto unit = manager.Create<Unit>();
  const auto types = manager.GetComponentTypes(unit);
  EXPECT_EQ(types, (std::vector{ComponentType::Transform, ComponentType::WorldMatrix})) << "a game component has no engine type, and is left out rather than named wrongly; the transform brought its world matrix";
  auto variant = manager.GetComponent(unit, ComponentType::Transform);
  ASSERT_TRUE(std::holds_alternative<Transform *>(variant));
  EXPECT_EQ(std::get<Transform *>(variant), manager.GetComponent<Transform>(unit));
  // One the entity does not have is a null pointer of that type, not some other component.
  auto missing = manager.GetComponent(unit, ComponentType::Camera);
  ASSERT_TRUE(std::holds_alternative<Camera *>(missing));
  EXPECT_EQ(std::get<Camera *>(missing), nullptr);
}
TEST(Flags, FindsWhatWasGivenAndWhatNothingReads) {
  constexpr Flag SPEED{"--speed", FlagKind::Value, "<n>", ""};
  constexpr Flag FAST{"--fast", FlagKind::Switch, "", ""};
  constexpr auto flags = JoinFlags(std::array{SPEED}, std::array{FAST});
  static_assert(flags.size() == 2 && HasDistinctNames(flags));
  static_assert(!HasDistinctNames(JoinFlags(flags, std::array{FAST})), "a flag given twice is caught at compile time");
  const std::vector<std::string> arguments{"--speed", "--fast", "--fats", "plain", "--fast"};
  EXPECT_TRUE(HasFlag(arguments, FAST));
  EXPECT_EQ(GetFlagValue(arguments, SPEED), "--fast") << "a value is whatever follows, even when it looks like a flag";
  EXPECT_EQ(FindUnknownFlags(arguments, flags), (std::vector<std::string>{"--fats"})) << "the value of --speed is not counted as a flag";
  EXPECT_FALSE(GetFlagValue(std::vector<std::string>{"--speed"}, SPEED));
}
TEST(Flags, NoTwoOfTheEnginesFlagsShareAName) {
  static_assert(HasDistinctNames(JoinFlags(ENGINE_FLAGS, NETWORK_FLAGS)));
  const auto described = DescribeFlags(NETWORK_FLAGS);
  EXPECT_NE(described.find("--host <port>"), std::string::npos);
}
