#pragma once
#include <bitset>
namespace kuki {
enum class GizmoType : uint8_t {
  Manipulator,
  ViewFrustum,
  FrustumCulling
};
using GizmoMask = std::bitset<static_cast<uint8_t>(GizmoType::FrustumCulling) + 1>;
} // namespace kuki
