#include <application.hpp>
#include <application_description.hpp>
#include <bounding_box.hpp>
#include <camera.hpp>
#include <game_builder.hpp>
#include <game_module.hpp>
#include <id.hpp>
#include <indirect_lighting.hpp>
#include <key.hpp>
#include <launch_path.hpp>
#include <light.hpp>
#include <light_type.hpp>
#include <material_asset.hpp>
#include <memory>
#include <mesh_asset.hpp>
#include <mesh_builder.hpp>
#include <spdlog/spdlog.h>
#include <my_game.hpp>
#include <my_game_world.hpp>
#include <spin_system.hpp>
#include <string>
#include <utility>
using namespace kuki;
using namespace my_game;
namespace {
/// Assets made in code have no file to be known by, so each gets an id made from a name, which a
/// saved scene can resolve again.
auto AddMesh(Application &app, const std::string &name, MeshBuilder &builder) -> void {
  auto asset = std::make_unique<MeshAsset>(MakeBuiltInAssetID("my_game/mesh/" + name));
  asset->mesh = builder.Take();
  asset->bounds = BoundingBox::Calculate(asset->mesh.vertices);
  app.AddAsset(std::move(asset), name);
}
} // namespace
MyGame::MyGame()
  : SystemApplication(ApplicationDescription{.name = "My Game"}) {}
auto MyGame::Start() -> void {
  SetupMyGame(*this);
}
auto SetupMyGame(Application &app) -> void {
  // The controls first, so the settings menu lists them in this order. Every action is a binding,
  // rebindable there; a key is only ever its default.
  app.RegisterBinding(REVERSE, Key::R, "Spin the other way");
  // The settings menu (Escape) is written in the overlay's font, so without one it opens unseen.
  // Staged beside the executable by `kuki_add_game(... FONT)`.
  if (!app.SetOverlayFont(ResolvePath("font/Inter-VariableFont_opsz,wght.ttf")))
    spdlog::warn("[MyGame] No font, so nothing will be written on screen");
  // One white material: the colour is the mesh's, by its vertices, so one material serves all.
  auto material = std::make_unique<MaterialAsset>(MakeBuiltInAssetID("my_game/material/painted"));
  material->type = MaterialType::Lit;
  material->fallback.albedo = glm::vec4(1.f);
  material->fallback.roughness = .8f;
  app.AddAsset(std::move(material), "Painted");
  MeshBuilder box;
  box.SetColor(glm::vec3{.85f, .35f, .2f}).Box({-.5f, .0f, -.5f}, {.5f, 1.f, .5f});
  AddMesh(app, "Box", box);
  MeshBuilder ground;
  ground.SetColor(glm::vec3{.45f, .6f, .35f}).Box({-6.f, -.1f, -6.f}, {6.f, .0f, 6.f});
  AddMesh(app, "Ground", ground);
  MyGameWorld::Game(app, "My Game")
    .Scene("Main")
    .Entity<CameraObject>("Camera")
    .At({.0f, 2.5f, 5.f})
    .Facing({.0f, -.4f, -1.f})
    .ActiveCamera()
    .Entity<LightObject>("Sun")
    .Facing({-.5f, -.7f, -.4f})
    .With<Light>([](Light &light) {
      light.type = LightType::Directional;
      light.intensity = 3.f;
    })
    .Entity<Sky>("Sky")
    .Entity<MeshObject>("Ground")
    .Mesh("Ground")
    .Material("Painted")
    .Entity<Spinner>("Box")
    .Mesh("Box")
    .Material("Painted")
    .With<Spin>([](Spin &spin) { spin.speed = 1.2f; });
}
// Once, in one source file of the game: what the launcher starts.
KUKI_GAME_MODULE("My Game", SetupMyGame)
KUKI_GAME_MAIN(MyGame)
