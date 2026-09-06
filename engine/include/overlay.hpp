#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <font.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float4.hpp>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <mesh.hpp>
#include <string>
#include <vector>
namespace kuki {
/// @brief Which point of the window a piece of overlay text is measured from.
///
/// The same point on the text is placed there, so the text grows into the window rather than out
/// of it: a `TopRight` caption keeps its right edge where it was put however long it becomes,
/// which is what a counter whose digits change width needs.
enum class TextAnchor : uint8_t {
  TopLeft,
  Top,
  TopRight,
  Left,
  Center,
  Right,
  BottomLeft,
  Bottom,
  BottomRight
};
/// @brief One run of text queued for this frame.
///
/// `offset` is in pixels from the anchor, x rightward and y downward -- the sense a window system
/// reports a cursor in, and the reason a caption pinned to the bottom right is nudged inwards with
/// negative numbers. `size` is the height of one line, also in pixels.
struct OverlayText {
  std::string text;
  glm::vec2 offset{};
  glm::vec4 color{1.f};
  float size{};
  TextAnchor anchor{TextAnchor::TopLeft};
  /// @brief What `Overlay::HitTest` answers with when the pointer is over this text.
  ///
  /// Chosen by the caller and meaningless here: a game that draws no clickable text leaves it at
  /// `NoHit` and never asks. This is the whole of the overlay's interaction model -- there are no
  /// widgets, no focus and no events, only "which of the things I drew is the pointer on", which
  /// is the one question a caller cannot answer for itself because only the overlay knows where
  /// the text ended up.
  int id{};
};
/// @brief Where a piece of text ended up, so that a later click can be matched against it.
struct OverlayHit {
  glm::vec2 min{};
  glm::vec2 max{};
  int id{};
};
/// @brief A stretch of the built mesh sharing one colour.
///
/// The overlay is drawn as one buffer and several draws rather than one draw, because colour is
/// the only thing that varies between runs and a vertex has nowhere to carry it: `Vertex` is the
/// scene's layout and gaining a colour for the sake of a caption would cost every mesh in every
/// scene twelve bytes a vertex. A handful of draws a frame is the cheaper side of that trade.
struct OverlayRun {
  size_t first{};
  size_t count{};
  glm::vec4 color{};
};
/// @brief Text queued for the current frame, and the font it is set in.
///
/// Immediate mode: whatever was asked for since the last frame was drawn is what appears, and the
/// queue is emptied once the frame that drew it is done. Nothing is retained, so a caption that
/// stops being asked for stops being drawn, and there is no handle to keep or free.
///
/// Submit from a script's `Update`. Systems run in the order `SystemApplication` was given them
/// and `RenderingSystem` comes after `ScriptingSystem`, so text queued from a script is drawn in
/// the same frame. Text queued from an application's own `Update`, which runs after the systems,
/// is drawn in the next one.
class KUKI_ENGINE_API Overlay {
public:
  /// @brief Name and id the baked atlas is registered under, so a backend can find it.
  ///
  /// An ordinary `TextureAsset` rather than anything the overlay uploads itself, which is most of
  /// why this class needs no graphics code: both backends already know how to get a texture asset
  /// onto the device, and the atlas rides that path like any other image.
  static constexpr const char *AtlasAssetName = "OverlayFont";
  auto SetFont(const std::filesystem::path &, const int = Font::DefaultPixelHeight) -> bool;
  auto GetFont() const -> const Font &;
  /// @brief The atlas texture asset, or an invalid id until a font has been set.
  auto GetAtlasAssetId() const -> AssetID;
  auto SetAtlasAssetId(const AssetID) -> void;
  /// @brief The id `HitTest` returns when the pointer is over no text that asked to be hit.
  static constexpr int NoHit = 0;
  /// @brief Queues a run of text for this frame. Ignored when no font has been set.
  ///
  /// Pass a non-zero `id` to make the text answer `HitTest`. Ids are the caller's to choose and
  /// need only be unique among the text drawn in one frame.
  auto DrawText(std::string, const glm::vec2 &, const float, const glm::vec4 & = glm::vec4(1.f), const TextAnchor = TextAnchor::TopLeft, const int = NoHit) -> void;
  /// @brief The id of the text under a point, or `NoHit`.
  ///
  /// Answers from where the last built frame put things, which is a frame behind whatever is being
  /// queued now. That is the right answer rather than a limitation: a click lands on what the
  /// person clicking could see, and what they could see is the frame that has been drawn.
  ///
  /// Takes a point in the same pixels `Build` was last given, with y measured downwards from the
  /// top -- the sense a window system reports a cursor in. `Application::PickOverlay` converts
  /// from window coordinates, which are not the same thing when the render target and the window
  /// have drifted apart.
  ///
  /// Later text wins where two overlap, matching the order they were drawn in.
  auto HitTest(const glm::vec2 &) const -> int;
  auto IsEmpty() const -> bool;
  auto Clear() -> void;
  /// @brief Lays this frame's text out in a target's pixels, as quads and the runs that colour them.
  ///
  /// Pixels with y measured upwards from the bottom, which is the space an orthographic projection
  /// of `(0, width, 0, height)` maps straight onto clip space in either backend. Both are handed
  /// the same mesh for that reason: the anchoring, the centring and the scaling are the same
  /// arithmetic whichever device ends up drawing it, and only the upload and the draw differ.
  auto Build(const int, const int, Mesh &, std::vector<OverlayRun> &) const -> void;
private:
  Font font;
  AssetID atlas{};
  std::vector<OverlayText> items;
  /// @brief Where the last `Build` put everything that asked to be hit, and the size it built at.
  ///
  /// Kept apart from `items` because it outlives them: the queue is emptied by the frame that drew
  /// it, and a click arriving afterwards still has to find what it landed on.
  mutable std::vector<OverlayHit> hits;
  mutable float builtHeight{};
};
} // namespace kuki
