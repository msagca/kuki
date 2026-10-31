#pragma once
#include <glm/vec4.hpp>
#include <material_type.hpp>
#include <texture_content.hpp>
namespace kuki {
/// @brief Surface values a material uses wherever it has no texture to read them from.
///
/// Split by how often it changes rather than by what it means. The colours and the surface scalars
/// are uploaded per instance, so two entities can share a mesh and a texture table and still differ
/// in colour; everything from `alphaCutoff` down is per material and reaches the shader another way.
/// The Direct3D root constants carry the whole struct either way, but the OpenGL backend feeds the
/// per-instance half through the instance attribute stream and the per-material half through plain
/// uniforms, because the attribute stream is full: locations 0 to 15 are all spoken for and
/// `GL_MAX_VERTEX_ATTRIBS` is 16. A new per-instance field has to displace one or move to a buffer.
///
/// `alphaCutoff` is only consulted when `alphaMode` is `Mask`, and the fourth component of `albedo`
/// is only consulted at all when `alphaMode` is not `Opaque`.
///
/// `attenuation` holds the volume's tint in its first three components and the distance light
/// travels through that volume before reaching it in the fourth; a distance of zero means the
/// volume does not absorb, whatever the tint says. `ior` also fixes the dielectric reflectance at
/// normal incidence, so it matters to every material, not only the transmissive ones: the familiar
/// 0.04 is what an index of 1.5 works out to.
struct MaterialFallback {
  glm::vec4 albedo{1.f};
  glm::vec4 specular{1.f};
  glm::vec4 emissive{.0f};
  glm::vec4 attenuation{1.f, 1.f, 1.f, .0f};
  float metalness{.5f};
  float occlusion{1.f};
  float roughness{.5f};
  TextureMask textureMask{0};
  float alphaCutoff{.5f};
  AlphaMode alphaMode{AlphaMode::Opaque};
  float transmission{.0f};
  float thickness{.0f};
  float ior{1.5f};
  /// @brief How far the surface is cel shaded rather than shaded physically, from nought to one.
  ///
  /// The shape of the bands is the scene's -- see `Stylization` -- and this is how much of them a
  /// surface takes. A figure drawn in flat tones in front of a backdrop shaded as a painting is
  /// the commonest reason to want both in one frame. Read by the Direct3D 12 backend; the OpenGL one
  /// shades every surface physically.
  float toon{.0f};
  /// @brief How far the scene's `Wind` bends the surface, per unit of height above its entity's
  /// origin squared, so a tree stood on its origin keeps its foot still and bends most at the top.
  ///
  /// A few hundredths for a tree a couple of units tall, a tenth or more for grass. Kept off the
  /// ground and off anything built: a material either sways or does not. Read by the Direct3D 12
  /// backend, in every pass that draws the surface, so its shadow and its outline move with it.
  float sway{.0f};
  /// @brief How much of the wind's waves the surface heaves with, from nought to one, and whether it
  /// is drawn faceted, a flat facet a triangle, the way low-poly water catches the light.
  ///
  /// Moves vertices up and down by where they are in the world, not by where they are in the mesh,
  /// so two pieces of the sea laid side by side heave as one. A wave is only as fine as the mesh it
  /// moves: the sea wants triangles about a unit across.
  float waves{.0f};
};
} // namespace kuki
