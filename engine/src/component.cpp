#include <component.hpp>
#include <component_type.hpp>
#include <string>
#include <unordered_map>
namespace kuki {
const std::unordered_map<ComponentType, std::string> Component::typeToName = {
  {ComponentType::Animator, "Animator"},
  {ComponentType::AntiAliasing, "Anti Aliasing"},
  {ComponentType::AtmosphereSky, "Atmosphere Sky"},
  {ComponentType::BoundingBox, "BoundingBox"},
  {ComponentType::Camera, "Camera"},
  {ComponentType::IndirectLighting, "Indirect Lighting"},
  {ComponentType::Light, "Light"},
  {ComponentType::MaterialHandle, "MaterialHandle"},
  {ComponentType::MeshHandle, "MeshHandle"},
  {ComponentType::ModelMaterialHandle, "ModelMaterialHandle"},
  {ComponentType::ModelMeshHandle, "ModelMeshHandle"},
  {ComponentType::ParticleEmitter, "Particle Emitter"},
  {ComponentType::PlanarReflection, "Planar Reflection"},
  {ComponentType::RenderBinding, "RenderBinding"},
  {ComponentType::ScreenEffects, "Screen Effects"},
  {ComponentType::Skeleton, "Skeleton"},
  {ComponentType::SkyboxHandle, "SkyboxHandle"},
  {ComponentType::Stylization, "Stylization"},
  {ComponentType::TextureHandle, "TextureHandle"},
  {ComponentType::Transform, "Transform"},
  {ComponentType::VolumetricClouds, "Volumetric Clouds"},
  {ComponentType::VolumetricFog, "Volumetric Fog"}};
auto Component::GetTypeName(const ComponentType type) -> std::string {
  if (auto it = typeToName.find(type); it != typeToName.end())
    return it->second;
  return {};
}
auto Component::IsSceneSingleton(const ComponentType type) -> bool {
  return type == ComponentType::AntiAliasing || type == ComponentType::AtmosphereSky || type == ComponentType::IndirectLighting || type == ComponentType::PlanarReflection || type == ComponentType::ScreenEffects || type == ComponentType::Stylization || type == ComponentType::VolumetricClouds || type == ComponentType::VolumetricFog;
}
} // namespace kuki
