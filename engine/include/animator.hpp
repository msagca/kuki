#pragma once
#include <component.hpp>
#include <kuki_engine_export.h>
namespace kuki {
struct KUKI_ENGINE_API Animator final : public IComponent {
  Animator()
    : IComponent(std::in_place_type<Animator>) {}
};
} // namespace kuki
