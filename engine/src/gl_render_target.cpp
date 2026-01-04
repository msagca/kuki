#include <gl_render_target.hpp>
namespace kuki {
GLRenderTarget::GLRenderTarget()
  : RenderTarget(std::in_place_type<GLRenderTarget>) {}
} // namespace kuki
