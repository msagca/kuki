#pragma once
#include <dx_material.hpp>
#include <gl_material.hpp>
#include <gl_mesh.hpp>
namespace kuki {
/// @brief What a backend has resolved an entity's mesh and material handles to.
///
/// One component for every backend, rather than a `GLMesh`, a `GLMaterial` and a `DXMaterial` each
/// added the first time a backend drew the entity. That was the renderers changing an entity's
/// archetype from inside a frame, which an entity's archetype no longer does: it is fixed when the
/// entity is made. So every archetype that is drawn carries one of these from the start
/// (`Renderable`, `ModelRenderable` in `engine_archetypes.hpp`), the backend that is running fills
/// in its own half, and the other half stays empty.
///
/// Not saved with the scene: it is derived from the handles beside it, and filled in again by
/// whichever backend loads the scene.
struct RenderBinding {
  GLMesh glMesh{};
  GLMaterial glMaterial{};
  /// Whether `glMaterial` has been filled in. A mesh is drawn only once it has a material, which is
  /// what the backend's draw loops asked of the two components when they were separate.
  bool glMaterialBound{};
  DXMaterial dxMaterial{};
  bool dxMaterialBound{};
};
} // namespace kuki
