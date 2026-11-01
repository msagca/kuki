// expect: a component must be trivially copyable
// A game's component that keeps a list of its own, which a table could not move or snapshot as bytes.
#include <component_traits.hpp>
#include <vector>
struct Inventory {
  std::vector<int> items;
};
KUKI_COMPONENT(Inventory, 0);
