#include <animator.hpp>
namespace kuki {
Animator::Animator()
  : IComponent(std::in_place_type<Animator>) {}
} // namespace kuki
