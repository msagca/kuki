#include <algorithm>
#include <overlay.hpp>
#include <utility>
namespace {
/// @brief Where an anchor sits, as a fraction of the window with both axes measured from the top
/// left.
///
/// One pair of numbers does both halves of the placement: it picks the point on the window and the
/// matching point on the text, which is what makes a right-hand anchor hold its right edge still.
auto AnchorFraction(const kuki::TextAnchor anchor) -> glm::vec2 {
  switch (anchor) {
  case kuki::TextAnchor::Top:
    return {.5f, .0f};
  case kuki::TextAnchor::TopRight:
    return {1.f, .0f};
  case kuki::TextAnchor::Left:
    return {.0f, .5f};
  case kuki::TextAnchor::Center:
    return {.5f, .5f};
  case kuki::TextAnchor::Right:
    return {1.f, .5f};
  case kuki::TextAnchor::BottomLeft:
    return {.0f, 1.f};
  case kuki::TextAnchor::Bottom:
    return {.5f, 1.f};
  case kuki::TextAnchor::BottomRight:
    return {1.f, 1.f};
  default:
    return {.0f, .0f};
  }
}
} // namespace
namespace kuki {
auto Overlay::SetFont(const std::filesystem::path &path, const int pixelHeight) -> bool {
  return font.Load(path, pixelHeight);
}
auto Overlay::GetFont() const -> const Font & {
  return font;
}
auto Overlay::GetAtlasAssetId() const -> AssetID {
  return atlas;
}
auto Overlay::SetAtlasAssetId(const AssetID id) -> void {
  atlas = id;
}
auto Overlay::DrawText(std::string text, const glm::vec2 &offset, const float size, const glm::vec4 &color, const TextAnchor anchor, const int id) -> void {
  if (!font.IsLoaded() || text.empty() || size <= .0f)
    return;
  items.push_back({std::move(text), offset, color, size, anchor, id});
}
auto Overlay::HitTest(const glm::vec2 &point) const -> int {
  // Into the space the rectangles were recorded in, which measures up from the bottom.
  const glm::vec2 flipped{point.x, builtHeight - point.y};
  // Backwards, so the last thing drawn is the first thing hit -- the same order the eye resolves
  // two overlapping captions in.
  for (auto it = hits.rbegin(); it != hits.rend(); ++it)
    if (flipped.x >= it->min.x && flipped.x <= it->max.x && flipped.y >= it->min.y && flipped.y <= it->max.y)
      return it->id;
  return NoHit;
}
auto Overlay::IsEmpty() const -> bool {
  return items.empty();
}
auto Overlay::Clear() -> void {
  items.clear();
}
auto Overlay::Build(const int width, const int height, Mesh &mesh, std::vector<OverlayRun> &runs) const -> void {
  mesh.vertices.clear();
  mesh.indices.clear();
  runs.clear();
  hits.clear();
  builtHeight = static_cast<float>(height);
  if (!font.IsLoaded() || width <= 0 || height <= 0)
    return;
  const glm::vec2 extent{static_cast<float>(width), static_cast<float>(height)};
  for (const auto &item : items) {
    glm::vec2 min, max;
    // The ink rather than the line box. A clock is placed by where its digits are, and the line box
    // carries the ascender and descender of glyphs the string does not contain -- so anchoring on
    // it leaves the text sitting further from the corner than it was asked to, by an amount that
    // depends on the typeface rather than on anything the caller said.
    if (!font.Bounds(item.text, min, max))
      continue;
    const auto ink = (max - min) * item.size;
    const auto fraction = AnchorFraction(item.anchor);
    // Both flipped into y-up: the fraction and the offset are stated with y downwards, and what
    // comes out of here is measured from the bottom of the target.
    const glm::vec2 anchorPoint{extent.x * fraction.x + item.offset.x, extent.y * (1.f - fraction.y) - item.offset.y};
    const glm::vec2 corner{ink.x * fraction.x, ink.y * (1.f - fraction.y)};
    // `Append` lays the glyphs out in ems about the pen, so the pen is the anchor less the corner
    // of the ink box that lands on it, less wherever the ink starts relative to the pen.
    const auto pen = anchorPoint - corner - min * item.size;
    const auto first = mesh.vertices.size();
    font.Append(item.text, mesh, {});
    for (auto i = first; i < mesh.vertices.size(); ++i) {
      auto &position = mesh.vertices[i].position;
      position = {position.x * item.size + pen.x, position.y * item.size + pen.y, .0f};
    }
    if (const auto count = mesh.vertices.size() - first; count > 0)
      runs.push_back({first, count, item.color});
    // The ink's box, padded to the full line. A row of options is picked at by pointing somewhere
    // near a word rather than exactly at its ink, and "5m" would otherwise be a shorter target
    // than "10m" and a much shorter one vertically than a label with a descender in it.
    if (item.id != NoHit) {
      const auto padding = item.size * .25f;
      const auto center = (min.y + max.y) * .5f * item.size + pen.y;
      const auto half = std::max(ink.y, item.size * .6f) * .5f + padding;
      hits.push_back({{pen.x + min.x * item.size - padding, center - half}, {pen.x + max.x * item.size + padding, center + half}, item.id});
    }
  }
}
} // namespace kuki
