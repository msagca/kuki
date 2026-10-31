#pragma once
#include <animator.hpp>
#include <anti_aliasing.hpp>
#include <atmosphere_sky.hpp>
#include <bounding_box.hpp>
#include <camera.hpp>
#include <component_traits.hpp>
#include <component_type.hpp>
#include <indirect_lighting.hpp>
#include <kuki_engine_export.h>
#include <light.hpp>
#include <material_handle.hpp>
#include <particle_emitter.hpp>
#include <planar_reflection.hpp>
#include <pool.hpp>
#include <screen_effects.hpp>
#include <skeleton.hpp>
#include <skybox_handle.hpp>
#include <string>
#include <stylization.hpp>
#include <texture_handle.hpp>
#include <transform.hpp>
#include <type_list.hpp>
#include <unordered_map>
#include <variant>
#include <volumetric_clouds.hpp>
#include <volumetric_fog.hpp>
#include <wind.hpp>
namespace kuki {
template <typename T>
using AddPointer = T *;
template <typename T>
using AddConstPointer = const T *;
/// @brief A pointer to any engine component, for code that learns the type at run time.
///
/// Generated from `EngineComponents`, so it cannot fall out of step with the enum.
using ComponentVariant = Apply<std::variant, Map<AddPointer, EngineComponents>>;
using ConstComponentVariant = Apply<std::variant, Map<AddConstPointer, EngineComponents>>;
class KUKI_ENGINE_API Component {
public:
  /// @brief Calls `func` with the `ComponentType` of each engine component in a signature.
  ///
  /// A game's components have no `ComponentType` and are skipped.
  static auto ForEachSetType(const ComponentMask &, auto &&) -> void;
  static auto GetTypeName(const ComponentType) -> std::string;
  /// @brief Whether a scene may hold at most one of this component, wherever it sits.
  ///
  /// These describe the scene rather than the entity carrying them: what its indirect lighting does,
  /// and in time whatever else is a property of the whole. An entity is only where such a thing is
  /// put so that it can be selected and edited like everything else, and two of them would be two
  /// answers to a question with one -- with nothing to say which the renderer should read.
  static auto IsSceneSingleton(const ComponentType) -> bool;
private:
  static const std::unordered_map<ComponentType, std::string> typeToName;
};
auto Component::ForEachSetType(const ComponentMask &mask, auto &&func) -> void {
  for (size_t i = 0; i < ENGINE_COMPONENT_COUNT; ++i)
    if (mask[i])
      func(static_cast<ComponentType>(i));
}
} // namespace kuki
