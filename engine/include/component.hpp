#pragma once
#include <bone_data.hpp>
#include <camera.hpp>
#include <component_type.hpp>
#include <gl_material.hpp>
#include <gl_mesh.hpp>
#include <gl_skybox.hpp>
#include <gl_texture.hpp>
#include <kuki_engine_export.h>
#include <light.hpp>
#include <material_handle.hpp>
#include <mesh_handle.hpp>
#include <skybox_handle.hpp>
#include <typeindex>
#include <unordered_map>
#include <variant>
namespace kuki {
using ComponentVariant = std::variant<BoneData *, Camera *, GLMaterial *, GLMesh *, GLSkybox *, GLTexture *, Light *, Transform *, MaterialHandle *, MeshHandle *, SkyboxHandle *, std::monostate>;
class KUKI_ENGINE_API Component {
public:
  static auto ForEachSetType(const ComponentMask &, auto &&) -> void;
  static auto ForEachUnsetType(const ComponentMask &, auto &&) -> void;
  static auto GetBit(const std::type_index &) -> size_t;
  static auto GetType(const std::type_index &) -> ComponentType;
  static auto GetTypeIndex(const ComponentType) -> std::type_index;
  static auto GetTypeName(const ComponentType) -> std::string;
private:
  static const std::unordered_map<std::type_index, ComponentType> indexToType;
  static const std::unordered_map<ComponentType, std::type_index> typeToIndex;
  static const std::unordered_map<ComponentType, std::string> typeToName;
};
auto Component::ForEachSetType(const ComponentMask &mask, auto &&func) -> void {
  for (auto i = 0; i < mask.size(); ++i)
    if (mask[i]) {
      const auto type = static_cast<ComponentType>(i);
      func(type);
    }
}
auto Component::ForEachUnsetType(const ComponentMask &mask, auto &&func) -> void {
  for (auto i = 0; i < mask.size(); ++i)
    if (!mask[i]) {
      const auto type = static_cast<ComponentType>(i);
      func(type);
    }
}
} // namespace kuki
