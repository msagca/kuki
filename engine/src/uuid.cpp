#include <array>
#include <cstdint>
#include <random>
#include <uuid.hpp>
namespace kuki {
UUID128::UUID128(uint64_t high, uint64_t low)
  : high(high), low(low) {}
const UUID128 UUID128::Invalid{0, 0};
UUID128 UUID128::Generate() {
  std::random_device rd;
  std::mt19937_64 gen(rd());
  std::array<uint8_t, 16> bytes;
  for (auto &b : bytes)
    b = static_cast<uint8_t>(gen() & 0xFF);
  bytes[6] = (bytes[6] & 0x0F) | 0x40;
  bytes[8] = (bytes[8] & 0x3F) | 0x80;
  UUID128 id;
  id.high = 0;
  id.low = 0;
  for (auto i = 0; i < 8; ++i)
    id.high = (id.high << 8) | bytes[i];
  for (auto i = 8; i < 16; ++i)
    id.low = (id.low << 8) | bytes[i];
  return id;
}
UUID128::operator bool() const {
  return *this != Invalid;
}
UUID128::operator int() const {
  return static_cast<int>(high ^ low);
}
} // namespace kuki
