// expect: a saved field must be a bool, a number, an enum
// A game saving a member that is an array, which a scene file has no one way to write.
#include <array>
#include <component_fields.hpp>
#include <component_traits.hpp>
struct Hand {
  int cards[5]{};
};
KUKI_COMPONENT(Hand, 0);
template <>
struct kuki::ComponentFields<Hand> {
  static constexpr std::array fields{kuki::Field<&Hand::cards>("cards")};
};
