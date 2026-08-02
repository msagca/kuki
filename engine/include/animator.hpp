#pragma once
#include <id.hpp>
namespace kuki {
struct Animator {
  AssetID modelAssetId{};
  int clipIndex{-1};
  float time{};
  bool playing{true};
  bool loop{true};
};
} // namespace kuki
