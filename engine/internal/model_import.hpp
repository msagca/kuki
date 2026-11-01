#pragma once
#include <filesystem>
#include <model_asset.hpp>
#include <string>
#include <unordered_map>
struct aiNode;
struct aiScene;
/// @brief Turning an Assimp scene into a `ModelAsset`, and nothing else.
///
/// These were private members of `AssetManager`, which meant its header -- reachable from almost
/// every other public header in the engine -- declared parameters in Assimp's own types and so had
/// to include four Assimp headers. Every consumer of the engine inherited that include path for
/// the sake of one loader.
///
/// Free functions rather than members because that is what they always were: not one of them reads
/// or writes any `AssetManager` state. They take what they need, call each other, and hand back a
/// filled-in `ModelAsset`.
///
/// Only the two entry points the asset manager calls are declared here. The rest -- the material,
/// mesh and texture importers and two small conversions -- stay inside `model_import.cpp`, because
/// their signatures name `aiMatrix4x4` and `aiTextureType`: a typedef of a template and an
/// unscoped enum, neither of which can be forward declared, so declaring them here would drag
/// Assimp's headers back in and undo the point of the move.
///
/// Internal either way. This header is not part of the installed public API.
namespace kuki {
/// @brief Recursively imports an Assimp node and its meshes into a `ModelAsset`.
///
/// @return Index of the new node within the model.
///
/// TODO: keep track of loaded meshes and reuse the indices
auto LoadNode(std::unordered_map<std::string, unsigned int> &, const aiNode &, const aiScene &, ModelAsset &, const std::filesystem::path & = {}, const std::string & = {}, int = -1) -> unsigned int;
auto ParseAnimations(const aiScene &, ModelAsset &) -> void;
} // namespace kuki
