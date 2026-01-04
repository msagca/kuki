#include <animation_system.hpp>
#include <application.hpp>
#include <system.hpp>
namespace kuki {
AnimationSystem::AnimationSystem()
  : System(std::in_place_type<AnimationSystem>) {}
AnimationSystem::~AnimationSystem() {
  Shutdown();
}
void AnimationSystem::Start() {
}
void AnimationSystem::Update(float deltaTime) {
}
void AnimationSystem::LateUpdate(float deltaTime) {
}
void AnimationSystem::Shutdown() {
}
} // namespace kuki
