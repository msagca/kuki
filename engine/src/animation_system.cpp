#include <animation_system.hpp>
#include <animator.hpp>
#include <application.hpp>
#include <cmath>
#include <glm/common.hpp>
#include <glm/gtc/quaternion.hpp>
#include <model_asset.hpp>
#include <skeleton.hpp>
#include <system.hpp>
#include <transform.hpp>
#include <utility>
namespace kuki {
namespace {
  auto SamplePosition(const std::vector<PositionKey> &keys, const float time) -> glm::vec3 {
    if (keys.empty())
      return {};
    if (keys.size() == 1 || time <= keys.front().time)
      return keys.front().value;
    if (time >= keys.back().time)
      return keys.back().value;
    for (size_t i = 0; i + 1 < keys.size(); ++i)
      if (time >= keys[i].time && time <= keys[i + 1].time) {
        const auto span = keys[i + 1].time - keys[i].time;
        const auto t = span > 0.f ? (time - keys[i].time) / span : 0.f;
        return glm::mix(keys[i].value, keys[i + 1].value, t);
      }
    return keys.back().value;
  }
  auto SampleScale(const std::vector<ScaleKey> &keys, const float time) -> glm::vec3 {
    if (keys.empty())
      return glm::vec3(1.f);
    if (keys.size() == 1 || time <= keys.front().time)
      return keys.front().value;
    if (time >= keys.back().time)
      return keys.back().value;
    for (size_t i = 0; i + 1 < keys.size(); ++i)
      if (time >= keys[i].time && time <= keys[i + 1].time) {
        const auto span = keys[i + 1].time - keys[i].time;
        const auto t = span > 0.f ? (time - keys[i].time) / span : 0.f;
        return glm::mix(keys[i].value, keys[i + 1].value, t);
      }
    return keys.back().value;
  }
  auto SampleRotation(const std::vector<RotationKey> &keys, const float time) -> glm::quat {
    if (keys.empty())
      return glm::quat(1.f, 0.f, 0.f, 0.f);
    if (keys.size() == 1 || time <= keys.front().time)
      return keys.front().value;
    if (time >= keys.back().time)
      return keys.back().value;
    for (size_t i = 0; i + 1 < keys.size(); ++i)
      if (time >= keys[i].time && time <= keys[i + 1].time) {
        const auto span = keys[i + 1].time - keys[i].time;
        const auto t = span > 0.f ? (time - keys[i].time) / span : 0.f;
        return glm::slerp(keys[i].value, keys[i + 1].value, t);
      }
    return keys.back().value;
  }
} // namespace
AnimationSystem::AnimationSystem(Application &app)
  : System(std::in_place_type<AnimationSystem>, app) {}
void AnimationSystem::Start() {}
void AnimationSystem::Update(const float deltaTime) {
  app.ForEachEntity<Animator, Skeleton>([&](const EntityID, Animator *animator, Skeleton *skeleton) {
    if (!animator || !skeleton || animator->clipIndex < 0)
      return;
    auto modelAsset = app.GetAsset<ModelAsset>(animator->modelAssetId);
    if (!modelAsset || animator->clipIndex >= static_cast<int>(modelAsset->animations.size()))
      return;
    const auto &clip = modelAsset->animations[animator->clipIndex];
    if (clip.duration <= 0.f || clip.ticksPerSecond <= 0.f)
      return;
    if (animator->playing) {
      animator->time += deltaTime * clip.ticksPerSecond;
      if (animator->time > clip.duration) {
        if (animator->loop)
          animator->time = std::fmod(animator->time, clip.duration);
        else {
          animator->time = clip.duration;
          animator->playing = false;
        }
      }
    }
    for (const auto &channel : clip.channels) {
      if (channel.nodeIndex >= skeleton->nodeEntities.size())
        continue;
      const auto boneEntityId = skeleton->nodeEntities[channel.nodeIndex];
      if (!boneEntityId)
        continue;
      auto *transform = app.GetEntityComponent<Transform>(boneEntityId);
      if (!transform)
        continue;
      if (!channel.positions.empty())
        transform->position = SamplePosition(channel.positions, animator->time);
      if (!channel.rotations.empty())
        transform->rotation = SampleRotation(channel.rotations, animator->time);
      if (!channel.scales.empty())
        transform->scale = SampleScale(channel.scales, animator->time);
      transform->dirty = true;
    }
  });
}
void AnimationSystem::Shutdown() {}
} // namespace kuki
