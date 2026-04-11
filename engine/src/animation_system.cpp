#include <animation_system.hpp>
#include <application.hpp>
#include <system.hpp>
namespace kuki {
AnimationSystem::AnimationSystem()
  : System(std::in_place_type<AnimationSystem>) {}
AnimationSystem::~AnimationSystem() {}
void AnimationSystem::Start() {}
void AnimationSystem::Update(const float) {}
void AnimationSystem::Shutdown() {}
} // namespace kuki
