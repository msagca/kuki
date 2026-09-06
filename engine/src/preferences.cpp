#include <charconv>
#include <fstream>
#include <launch_path.hpp>
#include <nlohmann/json.hpp>
#include <preferences.hpp>
#include <spdlog/spdlog.h>
#include <utility>
namespace {
/// @brief Turns an application's name into something safe to put in a filename.
///
/// Names come from `ApplicationDescription` and are written for people, so nothing stops one
/// carrying a slash or a colon. Anything that is not plainly a letter, a digit, a dash or an
/// underscore becomes an underscore, which cannot collide in any way that matters: two
/// applications differing only in punctuation would share a file, and two applications on one
/// machine differing only in punctuation is not a situation worth designing for.
auto SanitiseName(const std::string &name) -> std::string {
  std::string safe;
  safe.reserve(name.size());
  for (const auto c : name)
    safe.push_back((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ? c : '_');
  return safe.empty() ? std::string("Application") : safe;
}
} // namespace
namespace kuki {
auto Preferences::GetPath(const std::string &application) -> std::filesystem::path {
  return GetLaunchPath() / (SanitiseName(application) + ".prefs.json");
}
auto Preferences::Load(const std::string &application) -> bool {
  values.clear();
  path = GetPath(application);
  std::error_code error;
  if (!std::filesystem::exists(path, error))
    return false;
  std::ifstream stream(path);
  if (!stream) {
    spdlog::warn("[Preferences] Could not open {}", path.string());
    return false;
  }
  try {
    const auto json = nlohmann::json::parse(stream);
    if (!json.is_object())
      return false;
    for (const auto &[key, value] : json.items())
      if (value.is_string())
        values.emplace(key, value.get<std::string>());
  } catch (const nlohmann::json::parse_error &e) {
    // Dropped rather than repaired. A preferences file is a convenience, and the cost of a
    // corrupt one should be a game that starts with its defaults, not a game that does not start.
    spdlog::warn("[Preferences] {} is malformed and will be rewritten: {}", path.string(), e.what());
    values.clear();
    return false;
  }
  spdlog::info("[Preferences] Loaded {} ({} values)", path.string(), values.size());
  return true;
}
auto Preferences::Save() const -> bool {
  if (path.empty() || values.empty())
    return true;
  std::ofstream stream(path);
  if (!stream) {
    spdlog::warn("[Preferences] Could not write {}", path.string());
    return false;
  }
  nlohmann::json json = nlohmann::json::object();
  for (const auto &[key, value] : values)
    json[key] = value;
  stream << json.dump(2) << '\n';
  return static_cast<bool>(stream);
}
auto Preferences::Find(const std::string_view key) const -> const std::string * {
  const auto it = values.find(std::string(key));
  return it == values.end() ? nullptr : &it->second;
}
auto Preferences::Has(const std::string_view key) const -> bool {
  return Find(key) != nullptr;
}
auto Preferences::GetInt(const std::string_view key, const int fallback) const -> int {
  const auto *value = Find(key);
  if (!value)
    return fallback;
  auto result = fallback;
  const auto end = value->data() + value->size();
  const auto [stopped, error] = std::from_chars(value->data(), end, result);
  // The whole string or none of it: "5m" is not a 5 that happens to have something after it, it is
  // a value written by something that meant a different type.
  return error == std::errc{} && stopped == end ? result : fallback;
}
auto Preferences::GetFloat(const std::string_view key, const float fallback) const -> float {
  const auto *value = Find(key);
  if (!value)
    return fallback;
  auto result = fallback;
  const auto end = value->data() + value->size();
  const auto [stopped, error] = std::from_chars(value->data(), end, result);
  return error == std::errc{} && stopped == end ? result : fallback;
}
auto Preferences::GetBool(const std::string_view key, const bool fallback) const -> bool {
  const auto *value = Find(key);
  if (!value)
    return fallback;
  if (*value == "true")
    return true;
  if (*value == "false")
    return false;
  return fallback;
}
auto Preferences::GetString(const std::string_view key, const std::string &fallback) const -> std::string {
  const auto *value = Find(key);
  return value ? *value : fallback;
}
auto Preferences::Set(std::string key, const int value) -> void {
  values[std::move(key)] = std::to_string(value);
}
auto Preferences::Set(std::string key, const float value) -> void {
  values[std::move(key)] = std::to_string(value);
}
auto Preferences::Set(std::string key, const bool value) -> void {
  values[std::move(key)] = value ? "true" : "false";
}
auto Preferences::Set(std::string key, std::string value) -> void {
  values[std::move(key)] = std::move(value);
}
auto Preferences::Remove(const std::string_view key) -> bool {
  return values.erase(std::string(key)) > 0;
}
auto Preferences::Clear() -> void {
  values.clear();
}
auto Preferences::IsEmpty() const -> bool {
  return values.empty();
}
} // namespace kuki
