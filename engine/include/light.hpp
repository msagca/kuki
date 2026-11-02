#pragma once
#include <glm/ext/vector_float3.hpp>
#include <light_type.hpp>
namespace kuki {
/// @brief A light, placed and aimed by its entity's `Transform`: it shines down the entity's -Z.
///
/// Read from the entity's `WorldMatrix` (`WorldPosition`, `WorldForward`), so a light hung under
/// something moves with it. It kept a position, a rotation and the basis that followed from them,
/// apart from the entity's transform and synced with nothing; a game set `forward` alone and left the
/// rotation saying something else, which the scene file then had to guess between. Aim one with
/// `LookRotation`, or the builder's `Facing`.
struct Light {
  LightType type{LightType::Directional};
  glm::vec3 ambient{.2f};
  glm::vec3 diffuse{.5f};
  glm::vec3 specular{1.f};
  float intensity{1.f};
  float constant{1.f};
  float linear{.09f};
  float quadratic{.032f};
  float innerCutoff{.91f};
  float outerCutoff{.82f};
  float nearPlane{.1f};
  float farPlane{100.f};
  float orthoSize{10.f};
  /// @brief How wide a directional light is in the sky, as the angle it subtends in degrees.
  ///
  /// What gives its shadows a penumbra that widens with distance: sharp where a thing meets the
  /// surface it stands on, soft where the shadow has fallen a long way. The sun is a little over
  /// half a degree. A directional light has no position to be a size at, so an angle is the only
  /// form its size can take -- and it is the form in which the same light throws the same shadow
  /// whatever the scale of the scene under it.
  ///
  /// Nought is a light with no size, and the shadows it casts are the fixed-width ones every light
  /// cast before this existed. No value makes a shadow harder than that: the map's own texels have
  /// to be filtered away however thin the true penumbra is. Read only by a directional light.
  float angularDiameter{.53f};
  /// @brief The radius of a spot light's source, in scene units.
  ///
  /// The same thing for a light that does have a position: a bulb this big rather than a point. The
  /// penumbra it gives is this, times how much further the receiver is than the occluder, over how
  /// far the occluder is -- so it is the light's size against the distances in the scene that sets
  /// the softness, and a figure that suits a room will be far too small for a courtyard.
  ///
  /// Nought is a point, with the fixed-width shadow. Read only by a spot light; a point light casts
  /// no shadow at all.
  float sourceRadius{.05f};
};
} // namespace kuki
