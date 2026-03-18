#include <gl_buffer.hpp>
namespace kuki {
GLBuffer::operator bool() const {
  return id != 0;
}
} // namespace kuki
