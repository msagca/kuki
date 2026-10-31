#include <application.hpp>
#include <array>
#include <cmath>
#include <format>
#include <glm/ext/matrix_transform.hpp>
#include <id.hpp>
#include <material_asset.hpp>
#include <memory>
#include <mesh_asset.hpp>
#include <mesh_builder.hpp>
#include <props.hpp>
#include <random.hpp>
using namespace kuki;
namespace frontier {
namespace {
  constexpr glm::vec3 UP{.0f, 1.f, .0f};
  struct NamedPaint {
    const char *name;
    Paint paint;
  };
  /// @brief Every surface in the game, by name. The ground's colours are a little less saturated
  /// than the props', so a house reads in front of the field it stands in.
  const std::vector<NamedPaint> &Paints() {
    static const std::vector<NamedPaint> paints{
      // The ground: a top for each terrain, and the faces of the steps between them.
      {"ground/deep", {.albedo = {.02f, .07f, .12f}, .roughness = .9f, .toon = .5f}},
      {"ground/shallows", {.albedo = {.42f, .4f, .26f}, .roughness = .9f, .toon = .5f}},
      {"ground/sand", {.albedo = {.78f, .66f, .42f}, .roughness = .95f, .toon = .75f}},
      {"ground/grass", {.albedo = {.24f, .46f, .13f}, .roughness = .95f, .toon = .75f}},
      {"ground/meadow", {.albedo = {.42f, .55f, .16f}, .roughness = .95f, .toon = .75f}},
      {"ground/forest", {.albedo = {.13f, .26f, .08f}, .roughness = .95f, .toon = .75f}},
      {"ground/hills", {.albedo = {.42f, .44f, .2f}, .roughness = .95f, .toon = .75f}},
      {"ground/rock", {.albedo = {.33f, .31f, .3f}, .roughness = .9f, .toon = .8f}},
      {"ground/snow", {.albedo = {.86f, .9f, .97f}, .roughness = .8f, .toon = .85f}},
      {"cliff/earth", {.albedo = {.33f, .2f, .11f}, .roughness = .95f, .toon = .8f}},
      {"cliff/sand", {.albedo = {.6f, .46f, .28f}, .roughness = .95f, .toon = .8f}},
      {"cliff/stone", {.albedo = {.27f, .25f, .25f}, .roughness = .9f, .toon = .8f}},
      {"cliff/seabed", {.albedo = {.2f, .2f, .16f}, .roughness = .95f, .toon = .5f}},
      // The sea: opaque, so it is in the depth the reflection and the ink read, and tinted by how
      // deep it is over, since it cannot be seen through.
      {"water/deep", {.albedo = {.012f, .09f, .16f}, .roughness = .08f, .toon = .55f, .waves = 1.f, .metalness = .0f}},
      // The open sea far out, in tiles too large to carry a wave: heaved, each would catch the sun as
      // one great facet. Still, it reads as the same sea at that distance.
      {"water/far", {.albedo = {.012f, .09f, .16f}, .roughness = .12f, .toon = .55f}},
      {"water/shallows", {.albedo = {.03f, .26f, .3f}, .roughness = .08f, .toon = .55f, .waves = 1.f}},
      {"water/coast", {.albedo = {.05f, .34f, .36f}, .roughness = .1f, .toon = .6f, .waves = 1.f}},
      {"water/foam", {.albedo = {.95f, .98f, 1.f}, .toon = .0f, .waves = 1.f, .alpha = .55f, .unlit = true}},
      // The props.
      {"prop/bark", {.albedo = {.2f, .12f, .07f}, .roughness = .9f, .sway = .03f}},
      {"prop/pine", {.albedo = {.05f, .2f, .1f}, .roughness = .9f, .sway = .03f}},
      {"prop/leaves", {.albedo = {.2f, .45f, .1f}, .roughness = .9f, .sway = .035f}},
      {"prop/leaves_autumn", {.albedo = {.75f, .38f, .08f}, .roughness = .9f, .sway = .035f}},
      {"prop/bush", {.albedo = {.12f, .32f, .1f}, .roughness = .9f, .sway = .12f}},
      {"prop/berries", {.albedo = {.7f, .04f, .08f}, .roughness = .4f, .emissive = {.05f, .0f, .0f}, .sway = .12f}},
      {"prop/grass", {.albedo = {.3f, .58f, .14f}, .roughness = .9f, .toon = .7f, .sway = 1.6f}},
      {"prop/stone", {.albedo = {.45f, .44f, .42f}, .roughness = .85f}},
      {"prop/crystal", {.albedo = {.3f, .85f, .95f}, .roughness = .2f, .emissive = {.4f, 1.6f, 2.f}}},
      {"prop/crystal_rock", {.albedo = {.22f, .2f, .26f}, .roughness = .85f}},
      // The buildings.
      {"build/plaster", {.albedo = {.85f, .8f, .68f}, .roughness = .9f}},
      {"build/stone", {.albedo = {.42f, .4f, .38f}, .roughness = .85f}},
      {"build/wood", {.albedo = {.36f, .21f, .1f}, .roughness = .85f}},
      {"build/dark", {.albedo = {.08f, .07f, .07f}, .roughness = .7f}},
      {"build/roof_red", {.albedo = {.6f, .12f, .07f}, .roughness = .6f}},
      {"build/roof_blue", {.albedo = {.1f, .2f, .45f}, .roughness = .55f}},
      {"build/roof_thatch", {.albedo = {.62f, .48f, .2f}, .roughness = .95f}},
      {"build/vermilion", {.albedo = {.7f, .1f, .03f}, .roughness = .55f}},
      {"build/soil", {.albedo = {.25f, .15f, .08f}, .roughness = .95f}},
      {"build/wheat", {.albedo = {.85f, .65f, .2f}, .roughness = .9f, .sway = 1.4f}},
      {"build/sail", {.albedo = {.9f, .88f, .8f}, .roughness = .9f}},
      {"build/window", {.albedo = {.9f, .7f, .35f}, .roughness = .4f, .emissive = {.9f, .55f, .2f}}},
      // The birds.
      {"bird/gull", {.albedo = {.92f, .93f, .95f}, .roughness = .8f}},
      {"bird/gull_tip", {.albedo = {.08f, .08f, .1f}, .roughness = .8f}},
      {"bird/swallow", {.albedo = {.06f, .07f, .12f}, .roughness = .7f}},
    };
    return paints;
  }
  auto Builder() -> MeshBuilder {
    return {};
  }
  auto Turned(const float angle, const glm::vec3 &axis, const glm::vec3 &at = glm::vec3(.0f)) -> glm::mat4 {
    return glm::rotate(glm::translate(glm::mat4(1.f), at), angle, axis);
  }
  /// @brief A blade, a thin double-sided triangle from a foot to a tip: seen from both sides, since
  /// a blade of grass has no back.
  auto Blade(MeshBuilder &mesh, const glm::vec3 &foot, const glm::vec3 &tip, const float width) -> void {
    const auto along = tip - foot;
    auto across = glm::normalize(glm::cross(along, UP)) * width;
    if (!std::isfinite(across.x))
      across = {width, .0f, .0f};
    mesh.Triangle(foot - across, foot + across, tip);
    mesh.Triangle(foot + across, foot - across, tip);
  }
  auto MakeTrees(Application &app) -> void {
    for (auto v = 0; v < VARIANTS; ++v) {
      Random random(static_cast<uint64_t>(v) + 11u);
      const auto scale = random.Range(.85f, 1.2f);
      // A pine: a trunk and three cones, each turned a little from the last.
      auto trunk = Builder();
      trunk.Frustum({}, .07f * scale, .045f * scale, .4f * scale, 5);
      AddMesh(app, Prop("pine", v, "trunk"), trunk.Take());
      auto crown = Builder();
      auto y = .25f * scale;
      auto radius = .4f * scale;
      for (auto tier = 0; tier < 3; ++tier) {
        const auto height = (.55f - tier * .05f) * scale;
        crown.Frustum({.0f, y, .0f}, radius, .0f, height, 6 + (tier == 0 ? 1 : 0), random.Range(.0f, 1.f));
        y += height * .52f;
        radius *= .74f;
      }
      AddMesh(app, Prop("pine", v, "crown"), crown.Take());
      // A broadleaf: a taller trunk with a fork, and a crown of two or three stones of leaves.
      auto stem = Builder();
      stem.Frustum({}, .075f * scale, .05f * scale, .55f * scale, 5);
      stem.SetTransform(Turned(.5f, {.0f, .0f, 1.f}, {.0f, .4f * scale, .0f}));
      stem.Frustum({}, .035f * scale, .02f * scale, .3f * scale, 4);
      AddMesh(app, Prop("broad", v, "trunk"), stem.Take());
      auto leaves = Builder();
      leaves.Rock({.0f, .85f * scale, .0f}, glm::vec3(.42f, .36f, .42f) * scale, .18f, static_cast<uint32_t>(v * 7 + 1));
      leaves.Rock({.22f * scale, .7f * scale, .1f * scale}, glm::vec3(.26f, .22f, .26f) * scale, .2f, static_cast<uint32_t>(v * 7 + 2));
      if (random.Chance(.6f))
        leaves.Rock({-.2f * scale, .72f * scale, -.12f * scale}, glm::vec3(.24f, .2f, .24f) * scale, .2f, static_cast<uint32_t>(v * 7 + 3));
      AddMesh(app, Prop("broad", v, "crown"), leaves.Take());
    }
  }
  auto MakeUndergrowth(Application &app) -> void {
    for (auto v = 0; v < VARIANTS; ++v) {
      Random random(static_cast<uint64_t>(v) + 101u);
      auto bush = Builder();
      bush.Rock({.0f, .14f, .0f}, {.26f, .2f, .26f}, .22f, static_cast<uint32_t>(v + 40));
      bush.Rock({.16f, .1f, .08f}, {.15f, .13f, .15f}, .2f, static_cast<uint32_t>(v + 50));
      AddMesh(app, Prop("bush", v), bush.Take());
      auto berries = Builder();
      for (auto i = 0; i < 9; ++i) {
        const auto angle = random.Range(.0f, 6.2831853f);
        const auto up = random.Range(.05f, .3f);
        berries.Rock({std::cos(angle) * .24f, .1f + up, std::sin(angle) * .24f}, glm::vec3(.035f), .1f, static_cast<uint32_t>(i));
      }
      AddMesh(app, Prop("berries", v), berries.Take());
      // A tuft: blades round a foot, leaning out, of different heights.
      auto tuft = Builder();
      const auto blades = 6 + static_cast<int>(random.Below(4));
      for (auto i = 0; i < blades; ++i) {
        const auto angle = random.Range(.0f, 6.2831853f);
        const glm::vec3 out{std::cos(angle), .0f, std::sin(angle)};
        const auto height = random.Range(.14f, .3f);
        Blade(tuft, out * random.Range(.0f, .06f), out * random.Range(.06f, .14f) + glm::vec3(.0f, height, .0f), .022f);
      }
      AddMesh(app, Prop("tuft", v), tuft.Take());
      auto boulder = Builder();
      boulder.Rock({.0f, .12f, .0f}, {.34f, .26f, .3f}, .28f, static_cast<uint32_t>(v + 70));
      AddMesh(app, Prop("boulder", v), boulder.Take());
      auto pebbles = Builder();
      for (auto i = 0; i < 3; ++i) {
        const auto angle = random.Range(.0f, 6.2831853f);
        pebbles.Rock({std::cos(angle) * .25f, .04f, std::sin(angle) * .25f}, glm::vec3(random.Range(.08f, .13f)), .25f, static_cast<uint32_t>(i + v * 5 + 90));
      }
      AddMesh(app, Prop("pebbles", v), pebbles.Take());
      // Ore: a few crystals leaning out of a stone, each a hexagonal shaft with a point.
      auto crystals = Builder();
      auto base = Builder();
      base.Rock({.0f, .06f, .0f}, {.3f, .14f, .28f}, .25f, static_cast<uint32_t>(v + 120));
      for (auto i = 0; i < 5; ++i) {
        const auto angle = random.Range(.0f, 6.2831853f);
        const auto lean = random.Range(.15f, .6f);
        const glm::vec3 foot{std::cos(angle) * .12f, .08f, std::sin(angle) * .12f};
        const glm::vec3 axis{std::sin(angle), .0f, -std::cos(angle)};
        crystals.SetTransform(Turned(lean, axis, foot));
        const auto height = random.Range(.22f, .45f);
        const auto width = random.Range(.04f, .07f);
        crystals.Frustum({}, width, width, height, 6);
        crystals.Frustum({.0f, height, .0f}, width, .0f, width * 2.2f, 6);
      }
      AddMesh(app, Prop("crystals", v), crystals.Take());
      AddMesh(app, Prop("crystals", v, "base"), base.Take());
    }
  }
  /// @brief A bird whose beak is towards -Z, the way a camera looks, so it can be turned with the
  /// same look-at as one. The wings are their own meshes, hinged at the body's middle.
  auto MakeBirds(Application &app) -> void {
    auto body = Builder();
    const glm::vec3 nose{.0f, .0f, -.2f};
    const glm::vec3 tail{.0f, .01f, .14f};
    const glm::vec3 top{.0f, .045f, -.02f};
    const glm::vec3 belly{.0f, -.04f, -.02f};
    const glm::vec3 left{-.045f, .0f, -.02f};
    const glm::vec3 right{.045f, .0f, -.02f};
    for (const auto &end : {nose, tail})
      for (const auto &[a, b] : {std::pair{top, left}, std::pair{left, belly}, std::pair{belly, right}, std::pair{right, top}})
        body.Oriented(end, a, b, (a + b) * .5f + (end - glm::vec3(.0f, .0f, -.02f)) * .2f);
    // A forked tail, flat.
    body.Triangle(tail, tail + glm::vec3(-.07f, .0f, .1f), tail + glm::vec3(.0f, .0f, .05f));
    body.Triangle(tail, tail + glm::vec3(.0f, .0f, .05f), tail + glm::vec3(-.07f, .0f, .1f));
    body.Triangle(tail, tail + glm::vec3(.0f, .0f, .05f), tail + glm::vec3(.07f, .0f, .1f));
    body.Triangle(tail, tail + glm::vec3(.07f, .0f, .1f), tail + glm::vec3(.0f, .0f, .05f));
    AddMesh(app, "bird/body", body.Take());
    for (const auto side : {-1.f, 1.f}) {
      auto wing = Builder();
      const glm::vec3 front{.0f, .0f, -.06f};
      const glm::vec3 back{.0f, .0f, .07f};
      const glm::vec3 elbow{side * .2f, .0f, -.02f};
      const glm::vec3 tip{side * .42f, .0f, .1f};
      for (const auto facing : {1.f, -1.f}) {
        wing.Oriented(front, back, elbow, {.0f, facing, .0f});
        wing.Oriented(elbow, back, tip, {.0f, facing, .0f});
      }
      AddMesh(app, side < .0f ? "bird/wing_left" : "bird/wing_right", wing.Take());
    }
  }
  /// @brief The parts of every building, each drawn about the middle of the building's cells, with
  /// its long axis along +X and +Z the shape's second axis.
  auto MakeBuildings(Application &app) -> std::vector<BuildingType> {
    constexpr auto S = BUILDING_SPACING;
    std::vector<BuildingType> types;
    const auto Add = [&](BuildingType &type, const std::string &part, MeshBuilder &mesh, const std::string &material) {
      const auto name = "building/" + type.name + "/" + part;
      AddMesh(app, name, mesh.Take());
      type.parts.push_back({name, material});
    };
    {
      BuildingType cottage{"Cottage", {{.0f, .0f}}};
      MeshBuilder stone;
      MeshBuilder walls;
      MeshBuilder roof;
      MeshBuilder wood;
      MeshBuilder windows;
      stone.Block({.0f, -.05f, .0f}, {.44f, .12f, .36f});
      stone.Block({.2f, .72f, -.12f}, {.055f, .16f, .055f});
      walls.Block({.0f, .3f, .0f}, {.36f, .24f, .27f});
      roof.Gable({.0f, .53f, .0f}, {.44f, .35f}, .32f);
      wood.Block({.0f, .17f, .275f}, {.07f, .12f, .012f});
      windows.Block({-.2f, .32f, .275f}, {.06f, .055f, .01f});
      windows.Block({.2f, .32f, .275f}, {.06f, .055f, .01f});
      Add(cottage, "stone", stone, "build/stone");
      Add(cottage, "walls", walls, "build/plaster");
      Add(cottage, "roof", roof, "build/roof_red");
      Add(cottage, "door", wood, "build/wood");
      Add(cottage, "windows", windows, "build/window");
      cottage.smokes = true;
      cottage.chimney = {.2f, .9f, -.12f};
      types.push_back(std::move(cottage));
    }
    {
      // A farmhouse on the first cell and its field on the second, the wheat in rows across it.
      BuildingType farm{"Farm", {{.0f, .0f}, {1.f, .0f}}};
      const auto h = -S * .5f;
      const auto f = S * .5f;
      MeshBuilder walls;
      MeshBuilder roof;
      MeshBuilder soil;
      MeshBuilder wheat;
      MeshBuilder wood;
      walls.Block({h, .27f, .0f}, {.32f, .25f, .25f});
      walls.Block({h, -.05f, .0f}, {.36f, .08f, .3f});
      roof.Gable({h, .5f, .0f}, {.4f, .33f}, .3f);
      soil.Block({f, -.02f, .0f}, {.55f, .07f, .52f});
      Random random(5u);
      for (auto row = -3; row <= 3; ++row)
        for (auto i = -4; i <= 4; ++i) {
          const glm::vec3 foot{f + static_cast<float>(i) * .12f, .05f, static_cast<float>(row) * .14f};
          for (auto blade = 0; blade < 3; ++blade) {
            const auto angle = random.Range(.0f, 6.2831853f);
            const glm::vec3 out{std::cos(angle) * .03f, .0f, std::sin(angle) * .03f};
            Blade(wheat, foot + out * .3f, foot + out + glm::vec3(.0f, random.Range(.18f, .26f), .0f), .018f);
          }
        }
      for (auto i = -4; i <= 4; ++i) {
        wood.Block({f + static_cast<float>(i) * .135f, .1f, .56f}, {.015f, .1f, .015f});
        wood.Block({f + static_cast<float>(i) * .135f, .1f, -.56f}, {.015f, .1f, .015f});
      }
      wood.Block({f, .14f, .56f}, {.56f, .012f, .012f});
      wood.Block({f, .14f, -.56f}, {.56f, .012f, .012f});
      wood.Block({h, .16f, .255f}, {.065f, .11f, .012f});
      Add(farm, "walls", walls, "build/plaster");
      Add(farm, "roof", roof, "build/roof_thatch");
      Add(farm, "field", soil, "build/soil");
      Add(farm, "wheat", wheat, "build/wheat");
      Add(farm, "wood", wood, "build/wood");
      types.push_back(std::move(farm));
    }
    {
      // A long shed and the logs stacked beside it.
      BuildingType mill{"Lumber Mill", {{.0f, .0f}, {1.f, .0f}}};
      MeshBuilder shed;
      MeshBuilder roof;
      MeshBuilder logs;
      MeshBuilder stone;
      stone.Block({-.25f, -.05f, .0f}, {.7f, .1f, .38f});
      shed.Block({-.25f, .26f, .0f}, {.6f, .22f, .32f});
      roof.Gable({-.25f, .48f, .0f}, {.68f, .4f}, .25f);
      for (auto layer = 0; layer < 3; ++layer)
        for (auto i = 0; i < 3 - layer; ++i) {
          logs.SetTransform(Turned(1.5707963f, {1.f, .0f, .0f}, {.75f + (static_cast<float>(i) - (2 - layer) * .5f) * .14f, .07f + layer * .12f, -.32f}));
          logs.Frustum({}, .065f, .065f, .64f, 6);
        }
      Add(mill, "stone", stone, "build/stone");
      Add(mill, "shed", shed, "build/wood");
      Add(mill, "roof", roof, "build/dark");
      Add(mill, "logs", logs, "prop/bark");
      types.push_back(std::move(mill));
    }
    {
      BuildingType tower{"Watchtower", {{.0f, .0f}}};
      MeshBuilder stone;
      MeshBuilder wood;
      MeshBuilder roof;
      stone.Frustum({.0f, -.1f, .0f}, .32f, .25f, 1.5f, 8);
      wood.Frustum({.0f, 1.4f, .0f}, .38f, .38f, .08f, 8);
      for (auto i = 0; i < 8; ++i) {
        const auto angle = static_cast<float>(i) / 8.f * 6.2831853f;
        wood.Block({std::cos(angle) * .34f, 1.6f, std::sin(angle) * .34f}, {.025f, .12f, .025f});
      }
      roof.Frustum({.0f, 1.72f, .0f}, .44f, .0f, .5f, 8);
      Add(tower, "stone", stone, "build/stone");
      Add(tower, "wood", wood, "build/wood");
      Add(tower, "roof", roof, "build/roof_red");
      types.push_back(std::move(tower));
    }
    {
      // The sails turn about an axle along +Z, towards the front of the building: the placement
      // system spins them as hard as the wind blows.
      BuildingType windmill{"Windmill", {{.0f, .0f}}};
      MeshBuilder body;
      MeshBuilder cap;
      MeshBuilder sails;
      MeshBuilder wood;
      body.Frustum({.0f, -.1f, .0f}, .32f, .2f, 1.15f, 8);
      cap.Frustum({.0f, 1.05f, .0f}, .27f, .0f, .4f, 8);
      wood.Block({.0f, .2f, .3f}, {.08f, .14f, .02f});
      for (auto i = 0; i < 4; ++i) {
        const auto angle = static_cast<float>(i) * 1.5707963f;
        sails.SetTransform(Turned(angle, {.0f, .0f, 1.f}));
        sails.Block({.0f, .38f, .0f}, {.02f, .38f, .02f});
        sails.Quad({.03f, .12f, .01f}, {.18f, .14f, .01f}, {.18f, .74f, .01f}, {.03f, .74f, .01f});
        sails.Quad({.03f, .12f, .01f}, {.03f, .74f, .01f}, {.18f, .74f, .01f}, {.18f, .14f, .01f});
      }
      Add(windmill, "body", body, "build/plaster");
      Add(windmill, "cap", cap, "build/dark");
      Add(windmill, "door", wood, "build/wood");
      Add(windmill, "sails", sails, "build/sail");
      windmill.parts.back().spins = true;
      windmill.parts.back().pivot = {.0f, 1.0f, .3f};
      types.push_back(std::move(windmill));
    }
    {
      // A hall over three cells that meet at a corner, with a tower in the middle of its roof.
      BuildingType hall{"Town Hall", {{.0f, .0f}, {1.f, .0f}, {.5f, .87f}}};
      MeshBuilder stone;
      MeshBuilder walls;
      MeshBuilder roof;
      MeshBuilder wood;
      MeshBuilder windows;
      stone.Block({.0f, -.05f, .0f}, {.85f, .14f, .62f});
      walls.Block({.0f, .42f, .0f}, {.72f, .36f, .48f});
      walls.Block({.0f, 1.05f, .0f}, {.17f, .32f, .17f});
      roof.Gable({.0f, .78f, .0f}, {.84f, .6f}, .45f);
      roof.Frustum({.0f, 1.37f, .0f}, .27f, .0f, .42f, 4, .7853982f);
      for (auto i = -2; i <= 2; ++i) {
        wood.Block({static_cast<float>(i) * .3f, .42f, .485f}, {.025f, .36f, .012f});
        if (i != 2)
          windows.Block({static_cast<float>(i) * .3f + .15f, .5f, .485f}, {.07f, .1f, .01f});
      }
      wood.Block({.0f, .2f, .49f}, {.1f, .17f, .012f});
      Add(hall, "stone", stone, "build/stone");
      Add(hall, "walls", walls, "build/plaster");
      Add(hall, "roof", roof, "build/roof_blue");
      Add(hall, "wood", wood, "build/wood");
      Add(hall, "windows", windows, "build/window");
      hall.smokes = true;
      hall.chimney = {.5f, 1.f, -.3f};
      types.push_back(std::move(hall));
    }
    {
      // A small hall behind a gate, in vermilion.
      BuildingType shrine{"Shrine", {{.0f, .0f}}};
      MeshBuilder gate;
      MeshBuilder hall;
      MeshBuilder roof;
      MeshBuilder stone;
      for (const auto x : {-.27f, .27f})
        gate.Frustum({x, .0f, .3f}, .035f, .03f, .62f, 6);
      gate.Block({.0f, .64f, .3f}, {.38f, .03f, .045f});
      gate.Block({.0f, .54f, .3f}, {.3f, .02f, .03f});
      stone.Block({.0f, -.04f, -.1f}, {.3f, .1f, .25f});
      hall.Block({.0f, .2f, -.1f}, {.24f, .17f, .19f});
      roof.Gable({.0f, .36f, -.1f}, {.33f, .28f}, .22f);
      Add(shrine, "gate", gate, "build/vermilion");
      Add(shrine, "stone", stone, "build/stone");
      Add(shrine, "hall", hall, "build/wood");
      Add(shrine, "roof", roof, "build/dark");
      types.push_back(std::move(shrine));
    }
    // Each drawn about the middle of its own shape, which the meshes above were placed round by hand
    // for the one-cell buildings and by the half-spacing offsets for the others.
    return types;
  }
} // namespace
auto MaterialId(const std::string &name) -> AssetID {
  return MakeBuiltInAssetID("frontier/material/" + name);
}
auto MeshId(const std::string &name) -> AssetID {
  return MakeBuiltInAssetID("frontier/mesh/" + name);
}
auto AddMaterial(Application &app, const std::string &name, const Paint &paint) -> MaterialAsset * {
  auto *material = app.GetAsset<MaterialAsset>(MaterialId(name));
  if (!material) {
    auto created = std::make_unique<MaterialAsset>(MaterialId(name));
    material = created.get();
    app.AddAsset(std::move(created), name + "_surface");
  }
  material->type = paint.unlit ? MaterialType::Unlit : MaterialType::Lit;
  material->fallback.albedo = glm::vec4(paint.albedo, paint.alpha);
  material->fallback.emissive = glm::vec4(paint.emissive, 1.f);
  material->fallback.roughness = paint.roughness;
  material->fallback.metalness = paint.metalness;
  material->fallback.specular = glm::vec4(1.f);
  material->fallback.toon = paint.toon;
  material->fallback.sway = paint.sway;
  material->fallback.waves = paint.waves;
  material->fallback.alphaMode = paint.alpha < 1.f ? AlphaMode::Blend : AlphaMode::Opaque;
  return material;
}
auto AddMesh(Application &app, const std::string &name, Mesh mesh) -> MeshAsset * {
  const auto id = MeshId(name);
  auto *asset = app.GetAsset<MeshAsset>(id);
  if (!asset) {
    auto created = std::make_unique<MeshAsset>(id);
    asset = created.get();
    app.AddAsset(std::move(created), name);
  } else
    ++asset->revision;
  // An empty mesh has nothing to upload, and a backend asked to upload nothing complains every time.
  // One triangle of no size stands in for it and draws nothing.
  if (mesh.vertices.empty())
    mesh.vertices.resize(3);
  asset->mesh = std::move(mesh);
  asset->bounds = BoundingBox::Calculate(asset->mesh.vertices);
  return asset;
}
auto Prop(const std::string_view kind, const int variant, const std::string_view part) -> std::string {
  return part.empty() ? std::format("{}/{}", kind, variant) : std::format("{}/{}/{}", kind, variant, part);
}
auto LoadProps(Application &app) -> void {
  for (const auto &[name, paint] : Paints())
    AddMaterial(app, name, paint);
  MakeTrees(app);
  MakeUndergrowth(app);
  MakeBirds(app);
  Buildings(app);
}
auto Buildings(Application &app) -> const std::vector<BuildingType> & {
  static const auto types = MakeBuildings(app);
  return types;
}
} // namespace frontier
