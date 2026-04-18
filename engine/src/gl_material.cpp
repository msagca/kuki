#include <gl_material.hpp>
#include <gl_shader.hpp>
#include <glad/glad.h>
namespace kuki {
auto GLMaterial::Apply(const GLShader &shader) const -> void {
  if (type != shader.type)
    return;
  if (textures.albedo > 0)
    shader.SetTexture("material.albedo", textures.albedo);
  if (textures.normal > 0)
    shader.SetTexture("material.normal", textures.normal);
  if (textures.metalness > 0)
    shader.SetTexture("material.metalness", textures.metalness);
  if (textures.occlusion > 0)
    shader.SetTexture("material.occlusion", textures.occlusion);
  if (textures.roughness > 0)
    shader.SetTexture("material.roughness", textures.roughness);
  if (textures.specular > 0)
    shader.SetTexture("material.specular", textures.specular);
  if (textures.emissive > 0)
    shader.SetTexture("material.emissive", textures.emissive);
}
auto GLMaterial::operator==(const GLMaterial &other) const -> bool {
  const auto SameTexture = [this, &other](const int id, const int otherId, const TextureContent content) {
    const auto bit = static_cast<uint8_t>(content);
    const auto thisHas = fallback.textureMask.test(bit);
    const auto otherHas = other.fallback.textureMask.test(bit);
    if (thisHas != otherHas)
      return false;
    if (thisHas && id != otherId)
      return false;
    return true;
  };
  return type == other.type && SameTexture(textures.albedo, other.textures.albedo, TextureContent::Albedo) && SameTexture(textures.normal, other.textures.normal, TextureContent::Normal) && SameTexture(textures.metalness, other.textures.metalness, TextureContent::Metalness) && SameTexture(textures.occlusion, other.textures.occlusion, TextureContent::Occlusion) && SameTexture(textures.roughness, other.textures.roughness, TextureContent::Roughness) && SameTexture(textures.specular, other.textures.specular, TextureContent::Specular) && SameTexture(textures.emissive, other.textures.emissive, TextureContent::Emissive);
}
} // namespace kuki
