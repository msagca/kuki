// expect: two of a component's saved fields have the same key
// A game giving two members one key, so that the file could hold only one of them.
#include <archetype.hpp>
#include <array>
#include <component_fields.hpp>
#include <component_traits.hpp>
struct Wing {
  int flock{};
  int bird{};
};
KUKI_COMPONENT(Wing, 0);
template <>
struct kuki::ComponentFields<Wing> {
  static constexpr std::array fields{kuki::Field<&Wing::flock>("flock"), kuki::Field<&Wing::bird>("flock")};
};
auto Spec() -> kuki::ArchetypeSpec {
  return kuki::MakeArchetypeSpec<kuki::Archetype<Wing>>();
}
