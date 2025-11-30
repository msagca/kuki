#include <component.hpp>
namespace kuki {
IComponent::IComponent(const IComponent& other)
  : typeIndex(other.typeIndex) {}
} // namespace kuki
