#pragma once
#include <asset.hpp>
#include <kuki_engine_export.h>
namespace kuki {
struct KUKI_ENGINE_API SkyboxAsset final : public Asset {
  SkyboxAsset(const AssetID = AssetID::Invalid);
  int width{1024};
  int height{1024};
  int channels{3};
  std::vector<float> data{};
};
} // namespace kuki
