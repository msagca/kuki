#pragma once
#include <asset.hpp>
#include <kuki_engine_export.h>
#include <shader_type.hpp>
#include <string>
namespace kuki {
struct KUKI_ENGINE_API ShaderAsset final : public Asset {
  ShaderAsset(const AssetID = AssetID::Invalid);
  ShaderType type{ShaderType::Unknown};
  std::string text{};
};
} // namespace kuki
