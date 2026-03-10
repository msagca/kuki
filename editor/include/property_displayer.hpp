#pragma once
#include <bone_data.hpp>
#include <camera.hpp>
#include <editor.hpp>
#include <gl_material.hpp>
#include <gl_mesh.hpp>
#include <gl_skybox.hpp>
#include <gl_texture.hpp>
#include <light.hpp>
#include <material_handle.hpp>
#include <mesh_handle.hpp>
#include <skybox_handle.hpp>
#include <variant>
class PropertyDisplayer {
public:
  PropertyDisplayer(Editor &);
  auto operator()(kuki::BoneData *) -> void;
  auto operator()(kuki::Camera *) -> void;
  auto operator()(kuki::GLMaterial *) -> void;
  auto operator()(kuki::GLMesh *) -> void;
  auto operator()(kuki::GLSkybox *) -> void;
  auto operator()(kuki::GLTexture *) -> void;
  auto operator()(kuki::Light *) -> void;
  auto operator()(kuki::MaterialHandle *) -> void;
  auto operator()(kuki::MeshHandle *) -> void;
  auto operator()(kuki::SkyboxHandle *) -> void;
  auto operator()(kuki::TextureHandle *) -> void;
  auto operator()(kuki::Transform *) -> void;
  auto operator()(std::monostate) -> void;
private:
  Editor &editor;
};
