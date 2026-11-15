#pragma once
#include <array>
#include <cstdint>
#include <glm/ext/vector_float3.hpp>
#include <id.hpp>
#include <optional>
#include <string_view>
#include <vector>
namespace kuki {
class Application;
} // namespace kuki
namespace duel {
/// @brief The places a match can be fought in.
enum class StageId : uint8_t {
  /// @brief A back street at night in the rain, under neon.
  Alley,
  /// @brief A temple courtyard at sunset, blossom in the air.
  Temple,
  /// @brief A roof among the towers at dusk, the city lighting up.
  Rooftop,
  /// @brief A mountain shrine in falling snow, a fire burning.
  Shrine
};
inline constexpr std::array STAGE_NAMES{"alley", "temple", "rooftop", "shrine"};
inline constexpr auto STAGE_COUNT = STAGE_NAMES.size();
/// @brief The stage a name on the command line means, if it means one.
auto FindStage(const std::string_view) -> std::optional<StageId>;
/// @brief A light the stage's system keeps moving: a fire that flickers, a beacon that blinks, a
/// sign that stutters. Where it is and how bright it is meant to be, so it can be put back.
struct StageLight {
  enum class Kind : uint8_t {
    Steady,
    Flicker,
    Blink,
    Stutter
  };
  kuki::EntityID entity{};
  Kind kind{Kind::Steady};
  float intensity{};
};
/// @brief What a built stage leaves behind for its system: the entities to take away when another
/// stage is built, and what is to be kept moving while this one stands.
struct StageScene {
  StageId id{};
  std::vector<kuki::EntityID> entities;
  std::vector<StageLight> lights;
  /// @brief The sun's colour and strength as the stage set them, for an event -- a flash of
  /// lightning -- to return to.
  glm::vec3 sunColour{1.f};
  float sunIntensity{};
  /// @brief Whether lightning strikes here.
  bool storm{};
  /// @brief A glow that goes off with the blinking lights, where the stage has one.
  kuki::EntityID blinkGlow{};
};
/// @brief Loads every stage's parts and makes their materials. Once, before any stage is built.
auto LoadStages(kuki::Application &) -> void;
/// @brief Builds a stage into the scene, setting the sky, the air, the sun and the look to suit it.
///
/// The sky, the air, the clouds, the sun, the camera and the look are the scene's own and stay from
/// one stage to the next; a stage only sets them. Everything else it makes -- its parts, its lamps,
/// its weather -- is listed in what it returns, for whoever builds the next stage to take away.
auto BuildStage(kuki::Application &, const StageId) -> StageScene;
/// @brief Takes a stage's own entities out of the scene.
auto ClearStage(kuki::Application &, StageScene &) -> void;
} // namespace duel
