#pragma once
#include <filesystem>
#include <string>
namespace kuki {
struct ApplicationDescription {
  std::string name;
  std::filesystem::path path;
  std::filesystem::path iconPath;
};
} // namespace kuki
