#include <animation_system.hpp>
#include <application.hpp>
#include <system.hpp>
namespace kuki {
AnimationSystem::AnimationSystem(Application& app)
  : System(app) {}
AnimationSystem::~AnimationSystem() {
  Shutdown();
}
void AnimationSystem::Start() {
}
void AnimationSystem::Update(float deltaTime) {
}
void AnimationSystem::Shutdown() {
}
} // namespace kuki
