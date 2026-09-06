#pragma once
#include <array>
#include <cstddef>
#include <filesystem>
#include <glm/ext/vector_float2.hpp>
#include <kuki_engine_export.h>
#include <mesh.hpp>
#include <string_view>
#include <texture.hpp>
namespace kuki {
/// @brief Where one character sits in the atlas, and where it sits on the line.
///
/// Every measurement here is in ems rather than in the pixels the font was baked at, so that a
/// caller never has to know what that size was. One line of text is `Font::GetLineHeight` tall and
/// a string laid out with `Font::Append` comes out at that scale, which makes the entity's own
/// scale the only place the size of the text is decided.
///
/// `min` and `max` are the quad's corners relative to the pen, with y increasing upwards and the
/// baseline at zero -- so a descender is negative and a space is an empty box.
struct Glyph {
  glm::vec2 uvMin{};
  glm::vec2 uvMax{};
  glm::vec2 min{};
  glm::vec2 max{};
  float advance{};
};
/// @brief A font baked into one atlas, and the layout of a string across it.
///
/// The whole of the engine's text support, and deliberately less than a text renderer: this turns
/// a string into triangles and hands them back. What draws them is whatever the caller already
/// draws meshes with -- an entity with an atlas-textured material, which is why nothing here knows
/// about materials, entities or the screen. Text laid flat on a board and text pinned to a corner
/// of the window differ only in the transform applied to the same quads.
///
/// The quads come out in the XY plane facing +Z, wound counter-clockwise seen from there. That is
/// the plane a caller drawing a heads-up display wants unrotated, and one quarter turn about X
/// (`RotateEuler({-90, 0, 0})`) lays it face-up in the XZ plane with the text reading away from
/// the viewer, which is the one a caller writing on the ground wants.
///
/// ASCII only. A rank, a file and a clock are the cases in hand, an atlas covering more of Unicode
/// is a different size of problem, and a codepoint the atlas has no glyph for is skipped rather
/// than drawn as a box -- there is no box in the atlas to draw.
class KUKI_ENGINE_API Font {
public:
  static constexpr char FirstChar = ' ';
  static constexpr char LastChar = '~';
  static constexpr size_t CharCount = static_cast<size_t>(LastChar - FirstChar) + 1;
  /// @brief Size, in pixels, the glyphs are rasterised at when the caller states no preference.
  ///
  /// The one number that decides how the text holds up: an atlas is bitmap, so it is sharp at the
  /// size it was baked at and soft well above it. 48 covers a label a good fraction of a square
  /// tall on a 1080p window with room to be leaned into before it gives out.
  static constexpr int DefaultPixelHeight = 48;
  /// @brief Bakes every ASCII glyph of a TrueType file into one atlas.
  ///
  /// The atlas grows until the glyphs fit rather than being fixed, because the size that fits
  /// depends on the pixel height asked for and on how wide the face is, and guessing high wastes
  /// memory on every font that did not need it.
  ///
  /// @return Whether the file was read and packed. A failure is logged and leaves this unloaded.
  auto Load(const std::filesystem::path &, const int = DefaultPixelHeight) -> bool;
  auto IsLoaded() const -> bool;
  /// @brief The baked coverage, as an RGBA texture that is white everywhere and opaque on ink.
  ///
  /// Four channels for one channel of information, which is worth the waste: a single-channel
  /// texture arrives in the shader as `(r, 0, 0, 1)`, and it is the alpha that a cutout material
  /// tests. White beneath means the material's own albedo is what colours the text.
  auto GetAtlas() const -> const Texture &;
  /// @brief Distance between the baselines of two consecutive lines, in ems.
  auto GetLineHeight() const -> float;
  /// @brief How far the pen travels across a string, in ems. Newlines are not accounted for.
  auto Measure(const std::string_view) const -> float;
  /// @brief The box a string's ink occupies relative to the pen, in ems.
  ///
  /// Not the same box as the advance width and the line height describe, and the difference is the
  /// point: those two are what the next glyph and the next line need, while this is what the ink
  /// actually covers. Centring a label on a square wants this one -- "8" and "a" have the same line
  /// height and different amounts of ink, and centring on the line height leaves them looking as
  /// though they sit at different heights.
  ///
  /// @return Whether there was any ink. A blank or unrepresentable string leaves both untouched.
  auto Bounds(const std::string_view, glm::vec2 &, glm::vec2 &) const -> bool;
  /// @brief Appends `text`'s quads to a mesh, with the pen starting at `origin`.
  ///
  /// Appends rather than replaces, so that a caller with many strings to place -- eight files and
  /// eight ranks around a board, say -- can lay them all out in one plane and end up with a single
  /// mesh, one entity and one draw call.
  ///
  /// @return Where the pen finished, which is where a following call should start.
  auto Append(const std::string_view, Mesh &, const glm::vec2 & = {}) const -> glm::vec2;
private:
  auto Find(const char) const -> const Glyph *;
  std::array<Glyph, CharCount> glyphs{};
  Texture atlas{};
  float lineHeight{1.f};
  bool loaded{};
};
} // namespace kuki
