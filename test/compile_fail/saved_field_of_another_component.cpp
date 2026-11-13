// expect: a component's saved fields must be its own members
// A game copying one component's field list to another and leaving a member of the first in it.
#include <archetype.hpp>
#include <array>
#include <component_fields.hpp>
#include <component_traits.hpp>
struct Health {
  int points{};
};
struct Armour {
  int points{};
};
KUKI_COMPONENT(Health, 0);
KUKI_COMPONENT(Armour, 1);
template <>
struct kuki::ComponentFields<Armour> {
  static constexpr std::array fields{kuki::Field<&Health::points>("points")};
};
auto Spec() -> kuki::ArchetypeSpec {
  return kuki::MakeArchetypeSpec<kuki::Archetype<Armour>>();
}
