#include <buffer_description.hpp>
namespace kuki {
auto BufferDescription::operator==(const BufferDescription &other) const -> bool {
  return size == other.size;
}
} // namespace kuki
