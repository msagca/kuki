#include <gl_buffer.hpp>
#include <utility>
namespace kuki {
GLBuffer::GLBuffer()
  : BufferObject(std::in_place_type<GLBuffer>) {}
GLBuffer::operator bool() const {
  return id != 0;
}
} // namespace kuki
