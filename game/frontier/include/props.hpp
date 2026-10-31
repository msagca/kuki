#pragma once
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <id.hpp>
#include <mesh.hpp>
#include <string>
#include <string_view>
#include <vector>
namespace kuki {
class Application;
struct MaterialAsset;
struct MeshAsset;
} // namespace kuki
namespace frontier {
/// @brief What a surface is painted with. Colours are linear; an emissive above one blooms.
struct Paint {
  glm::vec3 albedo{1.f};
  float roughness{.8f};
  /// @brief How far it takes the cel bands. The props take most of them, the ground a little less.
  float toon{.85f};
  glm::vec3 emissive{.0f};
  float sway{};
  float waves{};
  /// @brief Below one draws it see-through, over everything solid.
  float alpha{1.f};
  bool unlit{};
  float metalness{};
};
/// @brief Registers a material under a name, or repaints the one already under it.
auto AddMaterial(kuki::Application &, const std::string &name, const Paint &) -> kuki::MaterialAsset *;
/// @brief Registers a mesh under a name, or replaces the one already under it and raises its
/// revision, so whatever draws it draws the new one.
auto AddMesh(kuki::Application &, const std::string &name, kuki::Mesh) -> kuki::MeshAsset *;
/// @brief The ids `AddMaterial` and `AddMesh` register under a name.
auto MaterialId(const std::string &name) -> kuki::AssetID;
auto MeshId(const std::string &name) -> kuki::AssetID;
/// @brief How many variants of each kind of prop there are, so no two neighbours look stamped.
inline constexpr int VARIANTS = 4;
/// @brief The name of one variant of a prop's part: `Prop("pine", 2, "crown")` is `pine/2/crown`.
auto Prop(const std::string_view kind, const int variant, const std::string_view part = "") -> std::string;
/// @brief Makes every material and every prop: trees, bushes, grass, flowers, stones, crystals and birds.
auto LoadProps(kuki::Application &) -> void;
/// @brief One piece of a building: a mesh and what it is painted with, and whether it turns.
struct BuildingPart {
  std::string mesh;
  std::string material;
  /// @brief A windmill's sails: turned about the building's local Z axis through `pivot`, as hard as
  /// the wind blows.
  bool spins{};
  glm::vec3 pivot{};
};
/// @brief A kind of building: the cells it takes and what it is made of.
struct BuildingType {
  std::string name;
  /// @brief The cells, in units of the grid's spacing along the building's own axes: the first is
  /// under the cursor. See `kuki::IrregularGrid::Footprint`.
  std::vector<glm::vec2> shape;
  std::vector<BuildingPart> parts;
  /// @brief Where its chimney smokes, in its own space, if it has one.
  bool smokes{};
  glm::vec3 chimney{};
};
/// @brief Every kind of building there is, its meshes made and registered on the first call.
auto Buildings(kuki::Application &) -> const std::vector<BuildingType> &;
/// @brief The spacing the buildings were drawn to: a building one cell across is about this wide.
inline constexpr float BUILDING_SPACING = 1.35f;
} // namespace frontier
