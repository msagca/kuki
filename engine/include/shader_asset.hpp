#pragma once
#include <asset.hpp>
#include <kuki_engine_export.h>
#include <material_type.hpp>
#include <shader_type.hpp>
#include <string>
namespace kuki {
struct KUKI_ENGINE_API ShaderAsset final : public Asset {
  ShaderAsset(const AssetID = AssetID::Invalid);
  ShaderType shaderType{ShaderType::Vertex};
  std::string text{};
  AssetID vertexShader{AssetID::Invalid};
  MaterialType materialType{MaterialType::Unlit};
};
} // namespace kuki
