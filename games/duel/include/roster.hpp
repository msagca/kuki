#pragma once
#include <array>
#include <glm/ext/vector_float3.hpp>
#include <string>
namespace duel {
/// @brief One of the fighters a player can pick.
///
/// Every fighter is the same rig with the same clips under a different mesh -- see
/// `tools/make_fighter.py` -- so the rules are the same whoever is picked, and the choice is
/// presentation only: two machines whose players picked different fighters play the same game.
struct Character {
  /// @brief The file `make_fighter.py`'s export wrote, without the extension: `aoi` is `aoi.glb`.
  const char *file;
  /// @brief What the select screen calls them, and the line under it.
  const char *name;
  const char *title;
  /// @brief What a second player who picks the same fighter wears instead, so the two can be told
  /// apart: the jacket, the trousers, the band and the gloves, linear.
  glm::vec3 jacket;
  glm::vec3 trousers;
  glm::vec3 band;
  glm::vec3 gloves;
};
inline constexpr std::array<Character, 4> ROSTER{{
  {"aoi", "AOI", "The steady hand", {.82f, .8f, .74f}, {.1f, .1f, .12f}, {.6f, .04f, .04f}, {.6f, .04f, .04f}},
  {"kaede", "KAEDE", "Faster than her shadow", {.05f, .3f, .32f}, {.03f, .04f, .05f}, {.95f, .9f, .85f}, {.7f, .7f, .72f}},
  {"goro", "GORO", "The mountain that walks", {.45f, .08f, .05f}, {.05f, .05f, .06f}, {.2f, .3f, .55f}, {.6f, .45f, .12f}},
  {"rin", "RIN", "Says nothing, misses nothing", {.82f, .8f, .74f}, {.25f, .2f, .14f}, {.08f, .18f, .4f}, {.75f, .74f, .7f}},
}};
/// @brief The name a fighter's model is registered under, and that of the recoloured copy a second
/// player who picks the same fighter is dressed in.
inline auto ModelName(const Character &character, const bool alternate = false) -> std::string {
  return std::string(character.file) + (alternate ? "_alternate" : ".glb");
}
} // namespace duel
