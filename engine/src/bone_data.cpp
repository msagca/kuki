#include <bone_data.hpp>
#include <component.hpp>
#include <utility>
namespace kuki {
BoneData::BoneData()
  : IComponent(std::in_place_type<BoneData>) {}
} // namespace kuki
