#include <algorithm>
#include <array>
#include <cstdint>
#include <font.hpp>
#include <fstream>
#include <spdlog/spdlog.h>
#include <stb_truetype.h>
#include <vector>
namespace {
/// @brief Edges the atlas is tried at, in order, until every glyph fits.
///
/// Three tries rather than a calculation, because what fits depends on the face as well as on the
/// pixel height and the packer is the only thing that knows for certain. Stopping at 2048 is not a
/// judgement about hardware -- an ASCII range that will not fit in four megapixels is a sign the
/// caller asked for a pixel height no atlas should be serving, and a bigger one would only make
/// the eventual failure slower.
constexpr int ATLAS_SIZES[]{512, 1024, 2048};
constexpr int LARGEST_ATLAS_SIZE = ATLAS_SIZES[2];
/// @brief Texels left between neighbouring glyphs, so that filtering cannot drag one into another.
constexpr int ATLAS_PADDING = 2;
/// @brief Samples per texel the rasteriser takes on each axis.
///
/// Text is the one thing on screen read rather than looked at, and at these sizes the difference
/// between a stem that is sampled twice per axis and one that is sampled once is the difference
/// between an `a` and a smudge. Costs four times the rasterisation, once, at load.
constexpr int ATLAS_OVERSAMPLE = 2;
auto ReadFile(const std::filesystem::path &path) -> std::vector<unsigned char> {
  std::ifstream fs(path, std::ios::binary | std::ios::ate);
  if (!fs)
    return {};
  const auto size = fs.tellg();
  if (size <= 0)
    return {};
  std::vector<unsigned char> bytes(static_cast<size_t>(size));
  fs.seekg(0);
  fs.read(reinterpret_cast<char *>(bytes.data()), size);
  if (!fs)
    return {};
  return bytes;
}
} // namespace
namespace kuki {
auto Font::Load(const std::filesystem::path &path, const int pixelHeight) -> bool {
  const auto pathNormStr = path.lexically_normal().string();
  loaded = false;
  if (pixelHeight <= 0) {
    spdlog::error("[Font] Pixel height must be positive: {}", pixelHeight);
    return false;
  }
  const auto file = ReadFile(path);
  if (file.empty()) {
    spdlog::error("[Font] Failed to read font file: {}", pathNormStr);
    return false;
  }
  stbtt_fontinfo info;
  if (!stbtt_InitFont(&info, file.data(), stbtt_GetFontOffsetForIndex(file.data(), 0))) {
    spdlog::error("[Font] Failed to parse font file: {}", pathNormStr);
    return false;
  }
  std::array<stbtt_packedchar, CharCount> packed{};
  std::vector<unsigned char> coverage;
  auto edge = 0;
  for (const auto size : ATLAS_SIZES) {
    coverage.assign(static_cast<size_t>(size) * size, 0);
    stbtt_pack_context context;
    if (!stbtt_PackBegin(&context, coverage.data(), size, size, 0, ATLAS_PADDING, nullptr))
      continue;
    stbtt_PackSetOversampling(&context, ATLAS_OVERSAMPLE, ATLAS_OVERSAMPLE);
    const auto fits = stbtt_PackFontRange(&context, file.data(), 0, static_cast<float>(pixelHeight), FirstChar, static_cast<int>(CharCount), packed.data());
    stbtt_PackEnd(&context);
    if (fits) {
      edge = size;
      break;
    }
  }
  if (!edge) {
    spdlog::error("[Font] {} glyphs of {} at {}px did not fit an atlas of {} texels a side", CharCount, pathNormStr, pixelHeight, LARGEST_ATLAS_SIZE);
    return false;
  }
  // Every measurement below is divided by this, which is what puts the glyphs in ems and keeps the
  // pixel height an implementation detail of how sharp the atlas is rather than a scale the caller
  // has to undo.
  const auto scale = static_cast<float>(pixelHeight);
  int ascent, descent, lineGap;
  stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
  lineHeight = static_cast<float>(ascent - descent + lineGap) * stbtt_ScaleForPixelHeight(&info, scale) / scale;
  for (size_t i = 0; i < CharCount; ++i) {
    auto x = .0f;
    auto y = .0f;
    stbtt_aligned_quad quad;
    stbtt_GetPackedQuad(packed.data(), edge, edge, static_cast<int>(i), &x, &y, &quad, 0);
    auto &glyph = glyphs[i];
    glyph.uvMin = {quad.s0, quad.t0};
    glyph.uvMax = {quad.s1, quad.t1};
    // stb measures down from the baseline and the mesh measures up from it, so the corners swap as
    // well as change sign: what stb calls the top of the glyph is the larger y here.
    glyph.min = {quad.x0 / scale, -quad.y1 / scale};
    glyph.max = {quad.x1 / scale, -quad.y0 / scale};
    glyph.advance = x / scale;
  }
  // The packer writes one byte of coverage per texel and the material wants four channels; white
  // underneath leaves the colour of the text to the material rather than baking one in here.
  std::vector<unsigned char> pixels(coverage.size() * 4, 0xFF);
  for (size_t i = 0; i < coverage.size(); ++i)
    pixels[i * 4 + 3] = coverage[i];
  atlas = {};
  atlas.content = TextureContent::Albedo;
  atlas.color = DefaultColorSpace(TextureContent::Albedo);
  atlas.channels = 4;
  atlas.width = edge;
  atlas.height = edge;
  atlas.data = std::move(pixels);
  loaded = true;
  spdlog::info("[Font] Baked {} glyphs of {} at {}px into a {}x{} atlas", CharCount, pathNormStr, pixelHeight, edge, edge);
  return true;
}
auto Font::IsLoaded() const -> bool {
  return loaded;
}
auto Font::GetAtlas() const -> const Texture & {
  return atlas;
}
auto Font::GetLineHeight() const -> float {
  return lineHeight;
}
auto Font::Find(const char c) const -> const Glyph * {
  if (c < FirstChar || c > LastChar)
    return nullptr;
  return &glyphs[static_cast<size_t>(c - FirstChar)];
}
auto Font::Measure(const std::string_view text) const -> float {
  auto width = .0f;
  if (!loaded)
    return width;
  for (const auto c : text)
    if (const auto *glyph = Find(c); glyph)
      width += glyph->advance;
  return width;
}
auto Font::Bounds(const std::string_view text, glm::vec2 &min, glm::vec2 &max) const -> bool {
  if (!loaded)
    return false;
  auto pen = .0f;
  auto found = false;
  for (const auto c : text) {
    const auto *glyph = Find(c);
    if (!glyph)
      continue;
    if (glyph->max.x > glyph->min.x && glyph->max.y > glyph->min.y) {
      const glm::vec2 low{pen + glyph->min.x, glyph->min.y};
      const glm::vec2 high{pen + glyph->max.x, glyph->max.y};
      min = found ? glm::vec2{std::min(min.x, low.x), std::min(min.y, low.y)} : low;
      max = found ? glm::vec2{std::max(max.x, high.x), std::max(max.y, high.y)} : high;
      found = true;
    }
    pen += glyph->advance;
  }
  return found;
}
auto Font::Append(const std::string_view text, Mesh &mesh, const glm::vec2 &origin) const -> glm::vec2 {
  auto pen = origin;
  if (!loaded)
    return pen;
  mesh.vertices.reserve(mesh.vertices.size() + text.size() * 6);
  for (const auto c : text) {
    if (c == '\n') {
      pen = {origin.x, pen.y - lineHeight};
      continue;
    }
    const auto *glyph = Find(c);
    if (!glyph)
      continue;
    // A space has an advance and no ink, and so has an empty box rather than a degenerate quad.
    if (glyph->max.x > glyph->min.x && glyph->max.y > glyph->min.y) {
      const auto min = pen + glyph->min;
      const auto max = pen + glyph->max;
      const auto Corner = [&](const float x, const float y, const float u, const float v) {
        return Vertex{.position = {x, y, .0f}, .normal = {.0f, .0f, 1.f}, .texture = {u, v}, .tangent = {1.f, .0f, .0f}};
      };
      // Two triangles wound counter-clockwise seen from +Z, which is the face the normal names.
      // The atlas's first row is its top, so the glyph's top edge takes the smaller v.
      const auto bottomLeft = Corner(min.x, min.y, glyph->uvMin.x, glyph->uvMax.y);
      const auto bottomRight = Corner(max.x, min.y, glyph->uvMax.x, glyph->uvMax.y);
      const auto topRight = Corner(max.x, max.y, glyph->uvMax.x, glyph->uvMin.y);
      const auto topLeft = Corner(min.x, max.y, glyph->uvMin.x, glyph->uvMin.y);
      mesh.vertices.insert(mesh.vertices.end(), {bottomLeft, bottomRight, topRight, bottomLeft, topRight, topLeft});
    }
    pen.x += glyph->advance;
  }
  return pen;
}
} // namespace kuki
