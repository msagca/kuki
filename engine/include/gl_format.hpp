#pragma once
#include <kuki_engine_export.h>
#include <target_description.hpp>
namespace kuki {
struct GLFormat {
  int external;
  int internal;
};
KUKI_ENGINE_API auto GLFormatToTarget(const unsigned int) -> TargetFormat;
KUKI_ENGINE_API auto TargetFormatToGL(const TargetFormat &) -> GLFormat;
/// @brief The texture target a description's storage is created against, and bound as ever after.
///
/// A texture name's target is fixed the first time it is bound, so everything that touches one --
/// the pool that allocates the storage, and the framebuffer attachments that name it -- has to
/// agree on which target that is. Said once here rather than derived at each of them, because they
/// disagreeing is not a visible error: `glFramebufferTexture2D` with the wrong target leaves an
/// incomplete framebuffer and nothing drawn.
///
/// The declared type decides it and the sample count does not, which is the part worth stating. A
/// `Texture2DMulti` target asked for a single sample is still a multisample texture with one sample
/// in it -- legal, and what the scene target becomes with antialiasing off. Reading the count
/// instead would call that a plain `GL_TEXTURE_2D` and attach it as one.
KUKI_ENGINE_API auto TargetTypeToGL(const TargetDescription &) -> unsigned int;
} // namespace kuki
