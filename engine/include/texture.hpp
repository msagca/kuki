#pragma once
#include <color.hpp>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <kuki_engine_export.h>
#include <string>
#include <texture_content.hpp>
#include <variant>
#include <vector>
namespace kuki {
/// @brief Block compression applied to a texture's pixels, or `None` while they are raw.
///
/// Only the formats a desktop GPU samples natively, so upload is a copy and the decode happens in
/// the texture unit. `stb_dxt` is the encoder, which is why the list stops where it does: BC7 would
/// be better for colour and BC6H is the only option for the HDR skyboxes, and it writes neither.
enum class TextureCompression : uint8_t {
  None,
  /// @brief Colour, no usable alpha. Four bits a pixel.
  BC1,
  /// @brief Colour with alpha. Eight bits a pixel.
  BC3,
  /// @brief One channel. Four bits a pixel, and what a greyscale mask should be.
  BC4,
  /// @brief Two channels. Eight bits a pixel, and what a tangent-space normal should be.
  BC5
};
/// @brief One mip level's place in the texture's byte blob.
///
/// Levels live end to end in one allocation rather than a vector each, because that is the shape
/// both upload paths want: Direct3D takes an array of subresources pointing into it, and OpenGL
/// walks it level by level.
struct TextureLevel {
  int width{};
  int height{};
  size_t offset{};
  size_t bytes{};
};
struct Texture {
  ColorRange range{ColorRange::LDR};
  ColorSpace color{ColorSpace::sRGB};
  TextureContent content{TextureContent::Albedo};
  int channels{3};
  int height{1024};
  int width{1024};
  bool flipY{false};
  /// @brief How `data` is encoded. `None` means raw pixels, `channels` wide.
  TextureCompression compression{TextureCompression::None};
  /// @brief The mip chain within `data`, or empty when `data` is a single level of raw pixels.
  std::vector<TextureLevel> levels;
  /// @brief File the pixels were decoded from, or empty when they exist only in memory.
  ///
  /// This is what makes `data` droppable. Decompressed pixels are the largest thing an asset holds
  /// and almost nothing needs them once they are on the GPU, so a texture that names its source can
  /// be uploaded, freed, and read back on the rare occasion something asks for pixels again.
  ///
  /// Textures embedded in a model file or generated at runtime leave this empty, and are never
  /// released, because nothing could bring them back.
  std::string source;
  std::variant<std::vector<unsigned char>, std::vector<float>> data;
};
/// @brief The space a content type's texels are authored in, which decides how they must be read.
///
/// Only the maps carrying a colour someone picked are authored through a display transfer curve.
/// The rest carry measurements, and a measurement put through that curve stops meaning what it
/// measured: a roughness of 0.5 read as sRGB arrives as 0.21, which is not a rougher or a smoother
/// surface but a different one, and every material in the scene comes out glossier than it was
/// authored. Whether that is visible depends on how the map compresses, which is the reason it went
/// unnoticed: a greyscale mask reaches BC4 and a normal map reaches BC5, and neither format has an
/// sRGB variant to be read through, so the damage was confined to the packed occlusion-roughness-
/// metalness maps that most glTF models ship, which are not greyscale and so land on BC1.
///
/// This settles two things at once, which is why it is one function rather than a flag set wherever
/// a texture is built: the internal format the texture uploads with, and the space its mips are
/// resampled in. Those disagreeing is worse than either being wrong on its own.
constexpr auto DefaultColorSpace(const TextureContent content) -> ColorSpace {
  switch (content) {
  case TextureContent::Normal:
  case TextureContent::Metalness:
  case TextureContent::Occlusion:
  case TextureContent::Roughness:
    return ColorSpace::Linear;
  default:
    return ColorSpace::sRGB;
  }
}
/// @brief Whether the texture is currently holding decompressed pixels.
/// @brief Finds a model's texture on this machine, whatever path the exporter happened to record.
///
/// Exporters routinely bake an absolute path from the authoring machine into the model file, and
/// FBX is the worst offender: it stores an absolute `FileName` beside the relative one, usually
/// pointing into an `.fbm` folder the FBX SDK extracted embedded media into, which no longer
/// exists anywhere. Joining that onto the model's own directory does not repair it. `operator/`
/// discards its left operand entirely when the right one is absolute, so what would be opened is
/// the authoring machine's path, unchanged and on a drive that may not even be mounted.
///
/// So a recorded path is treated as a hint rather than as an address. It is used directly when it
/// resolves, and otherwise only its file name is kept, which is then looked for beside the model
/// and in the conventional texture folders around it. That covers the layout the asset sites ship,
/// where the model sits in `source/` and its textures in a sibling `textures/`.
///
/// Separators are normalised first, so a Windows-authored path also resolves on a platform where a
/// backslash is an ordinary filename character rather than a separator.
///
/// @param modelDir Directory holding the model file, which every search is relative to.
/// @param recorded Path as the exporter wrote it, in any separator convention, absolute or not.
/// @return The resolved file, or an empty path when the texture is not on this machine at all.
auto KUKI_ENGINE_API ResolveTexturePath(const std::filesystem::path &, const std::string &) -> std::filesystem::path;
/// @brief Layout version of the baked texture format.
///
/// Written into every baked file and checked on read, so a change to the encoder, the mip rule or
/// the header retires the old bakes instead of feeding this build data it would misread. Bump it
/// whenever what `WriteCachedTexture` produces stops matching what an older build would produce.
inline constexpr uint32_t TEXTURE_CACHE_VERSION = 1;
/// @brief Where the baked form of a texture lives, or empty when it cannot be baked.
///
/// The name is a hash of everything the bake depends on, which is more than the file path: the
/// same image compresses to BC1 as colour and BC5 as a normal map, and mips are resampled in
/// gamma space for one and linear for the other. Keying on the path alone would let a material
/// that binds an image to one slot poison what a material binding it to another slot reads back.
auto KUKI_ENGINE_API GetTextureCachePath(const Texture &) -> std::filesystem::path;
/// @brief Loads a texture's baked mip chain, skipping decode, resampling and compression entirely.
///
/// A miss is silent and ordinary. Anything unexpected in the file is treated as a miss too, so a
/// truncated or half-written bake costs one wasted read rather than a crash or a corrupt upload.
///
/// @return True when `texture` now holds the baked chain and nothing else needs to run.
auto KUKI_ENGINE_API ReadCachedTexture(Texture &) -> bool;
/// @brief Writes a compressed texture's mip chain out so the next run can skip building it.
///
/// Written to a temporary file and renamed into place, because model textures are baked on loader
/// threads and two runs can race for the same file. A rename either happens or does not, so a
/// reader sees a complete bake or no bake, never a partial one.
auto KUKI_ENGINE_API WriteCachedTexture(const Texture &) -> bool;
/// @brief Gets a texture ready to upload, from the bake when there is one and from source when not.
///
/// The route the model loader takes. Compression costs on the order of a second per 4K texture and
/// produces the same bytes every time, so it is worth doing once per machine rather than once per
/// load. `texture.source`, `content` and `color` must already be set, since all three decide what
/// the bake contains and therefore which bake this is.
auto KUKI_ENGINE_API PrepareTextureCached(Texture &) -> bool;
auto KUKI_ENGINE_API HasTexturePixels(const Texture &) -> bool;
/// @brief Decodes `source`, replacing `data` along with the size, channel count and range it implies.
///
/// Leaves `content`, `color` and `flipY` alone: those are decisions the caller made about what the
/// image is for, not facts about the file, and a reload must not undo them.
auto KUKI_ENGINE_API ReadTexturePixels(Texture &) -> bool;
/// @brief Decodes `source` only when the texture is currently empty.
auto KUKI_ENGINE_API EnsureTexturePixels(Texture &) -> bool;
/// @brief Frees the pixels, but only when `source` can bring them back.
///
/// @return True when pixels were actually freed.
auto KUKI_ENGINE_API ReleaseTexturePixels(Texture &) -> bool;
/// @brief Builds a mip chain and block-compresses it, picking a format from what the texture is for.
///
/// Meant to run wherever the decode ran, which is off the main thread, because compression costs far
/// more than decoding does. Both backends consume the result, so the choice of format is made once
/// here rather than twice in the renderers.
///
/// Idempotent: a texture that already has levels is left alone. HDR and skybox textures are left
/// raw, the first because there is no encoder here that could hold them and the second because the
/// compute passes that turn an equirectangular map into a cubemap sample it directly. So are
/// textures whose sides are not multiples of four, which spares both backends the question of what
/// a partial edge block means to them.
///
/// The chain stops at the last level whose sides are still at least four, so it is shorter than a
/// full chain by two levels. Every level it does emit has the size its API derives for that index,
/// which is what lets a partial chain upload as though it were whole.
///
/// @return True when the texture came out compressed.
auto KUKI_ENGINE_API PrepareTexturePixels(Texture &) -> bool;
/// @brief The texture's levels, standing in a single level for a texture that has no chain.
auto KUKI_ENGINE_API GetTextureLevels(const Texture &) -> std::vector<TextureLevel>;
/// @brief Bytes one level of this compression occupies, counting the padding blocks at the edges.
auto KUKI_ENGINE_API GetCompressedLevelBytes(TextureCompression, int, int) -> size_t;
} // namespace kuki
