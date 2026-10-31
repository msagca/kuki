#include <engine_archetypes.hpp>
#include <entity_manager.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <gtest/gtest.h>
#include <pool.hpp>
#include <vector>
#include <world.hpp>
using namespace kuki;
namespace {
auto Declare(EntityManager &manager) -> void {
  for (const auto &spec : EngineWorld::Specs())
    manager.Declare(spec);
}
auto Key(const char *mesh, const char *material = "") -> PoolKey {
  return {.mesh = MakeBuiltInAssetID(mesh), .material = *material ? MakeBuiltInAssetID(material) : AssetID{}};
}
auto Pools(const EntityManager &manager) -> std::vector<const ArchetypeTable *> {
  std::vector<const ArchetypeTable *> pools;
  manager.ForEachPool([&](const ArchetypeTable &table) { pools.push_back(&table); });
  return pools;
}
} // namespace
TEST(Pool, KeepsEachMeshAndMaterialsEntitiesTogether) {
  EntityManager manager;
  Declare(manager);
  const auto rock = Key("rock", "stone");
  const auto tree = Key("tree", "bark");
  std::vector<EntityID> rocks;
  for (auto i = 0; i < 3; ++i) {
    rocks.push_back(manager.Create<MeshObject>(rock));
    manager.Create<MeshObject>(tree);
  }
  const auto pools = Pools(manager);
  ASSERT_EQ(pools.size(), 2u) << "one table a pool, however the entities were made in turn";
  for (const auto *pool : pools) {
    EXPECT_EQ(pool->entities.size(), 3u);
    ASSERT_NE(pool->GetColumn<WorldMatrix>(), nullptr);
    EXPECT_EQ(pool->GetColumn<WorldMatrix>()->components.size(), 3u) << "the world matrices are one block, in the order of the entities";
  }
  EXPECT_EQ(manager.GetPool(rocks[1]), rock);
  EXPECT_FALSE(manager.Create<MeshObject>("unpooled") && manager.GetPool(manager.GetID("unpooled"))) << "made with no mesh, an entity is in no pool";
}
TEST(Pool, MovesAnEntityToAnotherPoolAndKeepsItWhole) {
  EntityManager manager;
  Declare(manager);
  const auto first = manager.Create<MeshObject>(Key("rock", "stone"), "first");
  const auto second = manager.Create<MeshObject>(Key("rock", "stone"), "second");
  manager.GetComponent<Transform>(second)->position = {1.f, 2.f, 3.f};
  ASSERT_TRUE(manager.MoveToPool(second, Key("rock", "moss")));
  EXPECT_EQ(manager.GetPool(second), Key("rock", "moss")) << "given another material by moving";
  EXPECT_EQ(manager.GetName(second), "second") << "the same entity, by id and name";
  EXPECT_EQ(manager.GetComponent<Transform>(second)->position, glm::vec3(1.f, 2.f, 3.f)) << "and with its components";
  EXPECT_EQ(manager.GetPool(first), Key("rock", "stone"));
  for (const auto *pool : Pools(manager))
    EXPECT_EQ(pool->entities.size(), 1u) << "the pool it left closed up behind it";
  EXPECT_FALSE(manager.MoveToPool(manager.Create<EmptyNode>(), Key("rock"))) << "an archetype that is not drawn has no pools";
}
TEST(Pool, StaysOneBlockWhenAnEntityGoes) {
  EntityManager manager;
  Declare(manager);
  std::vector<EntityID> rocks;
  for (auto i = 0; i < 4; ++i)
    rocks.push_back(manager.Create<MeshObject>(Key("rock")));
  manager.Delete(rocks[1]);
  const auto pools = Pools(manager);
  ASSERT_EQ(pools.size(), 1u);
  EXPECT_EQ(pools[0]->entities.size(), 3u);
  for (const auto id : {rocks[0], rocks[2], rocks[3]})
    EXPECT_EQ(manager.GetPool(id), Key("rock")) << "the last row moved into the gap and is still found";
}
TEST(Pool, CopiesIntoTheSamePoolWithItsBounds) {
  EntityManager prefabs;
  Declare(prefabs);
  BoundingBox box;
  box.min = glm::vec3(-1.f);
  box.max = glm::vec3(1.f);
  ASSERT_TRUE(prefabs.CreatePool(MeshObject::Mask(), Key("rock"), {.bounds = box}));
  const auto prefab = prefabs.Create<MeshObject>(Key("rock"));
  EntityManager scene;
  Declare(scene);
  const auto placed = scene.CopyFrom(prefabs, prefab);
  EXPECT_EQ(scene.GetPool(placed), Key("rock"));
  EXPECT_EQ(scene.GetBounds(placed).max, box.max) << "the pool made by the copy has the bounds the prefab's had";
}
TEST(Pool, RaisesItsVersionWhenAnEntityMoves) {
  EntityManager manager;
  Declare(manager);
  const auto still = manager.Create<MeshObject>(Key("still"));
  const auto moving = manager.Create<MeshObject>(Key("moving"));
  manager.UpdateComponents<Transform>();
  const auto Version = [&](const PoolKey &key) {
    uint64_t version{};
    manager.ForEachPool([&](const ArchetypeTable &table) {
      if (table.pool == key)
        version = table.version;
    });
    return version;
  };
  const auto stillBefore = Version(Key("still"));
  const auto movingBefore = Version(Key("moving"));
  manager.GetComponent<Transform>(moving)->position.x = 5.f;
  manager.MarkTransformDirty(moving);
  manager.UpdateComponents<Transform>();
  EXPECT_EQ(Version(Key("still")), stillBefore) << "a pool none of whose entities moved is as it was";
  EXPECT_GT(Version(Key("moving")), movingBefore);
  EXPECT_FLOAT_EQ(manager.GetComponent<WorldMatrix>(moving)->matrix[3].x, 5.f);
  (void)still;
}
TEST(Pool, IsVisitedByAQueryOverItsArchetype) {
  EntityManager manager;
  Declare(manager);
  manager.Create<MeshObject>(Key("rock"));
  manager.Create<MeshObject>(Key("tree"));
  manager.Create<MeshObject>();
  auto viaOf = 0;
  manager.ForEachOf<MeshObject, Transform>([&](const EntityID, Transform *) { ++viaOf; });
  EXPECT_EQ(viaOf, 3) << "a query over one archetype visits its own table and every pool of it";
  auto viaAny = 0;
  manager.ForEach<Pooled>([&](const EntityID, Pooled *) { ++viaAny; });
  EXPECT_EQ(viaAny, 3);
}
