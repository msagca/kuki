#include <algorithm>
#include <animation_system.hpp>
#include <animator.hpp>
#include <application.hpp>
#include <cmath>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/matrix.hpp>
#include <glm/gtc/quaternion.hpp>
#include <model_asset.hpp>
#include <profiler.hpp>
#include <skeleton.hpp>
#include <string_view>
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
namespace {
  /// @brief A node's place in the model as it was built: what a bone no clip is moving goes back to.
  auto BindPose(const ModelNode &node) -> BonePose {
    BonePose pose;
    pose.position = glm::vec3(node.transform[3]);
    pose.scale = {glm::length(glm::vec3(node.transform[0])), glm::length(glm::vec3(node.transform[1])), glm::length(glm::vec3(node.transform[2]))};
    const glm::mat3 axes{glm::vec3(node.transform[0]) / std::max(pose.scale.x, 1e-8f), glm::vec3(node.transform[1]) / std::max(pose.scale.y, 1e-8f), glm::vec3(node.transform[2]) / std::max(pose.scale.z, 1e-8f)};
    pose.rotation = glm::quat_cast(axes);
    pose.set = true;
    return pose;
  }
  auto Blend(const BonePose &from, const BonePose &to, const float weight) -> BonePose {
    return {glm::mix(from.position, to.position, weight), glm::slerp(from.rotation, to.rotation, weight), glm::mix(from.scale, to.scale, weight), true};
  }
  /// @brief The clip a state plays, found by name the first time and by index after that.
  auto StateClip(AnimationState &state, const ModelAsset &model) -> const AnimationClip * {
    if (state.clipIndex < 0)
      for (size_t i = 0; i < model.animations.size() && state.clipIndex < 0; ++i) {
        const std::string_view name = model.animations[i].name;
        const auto bar = name.rfind('|');
        if (name == state.clip || (bar != std::string_view::npos && name.substr(bar + 1) == state.clip))
          state.clipIndex = static_cast<int>(i);
      }
    if (state.clipIndex < 0 || state.clipIndex >= static_cast<int>(model.animations.size()))
      return nullptr;
    const auto &clip = model.animations[state.clipIndex];
    return clip.duration > .0f && clip.ticksPerSecond > .0f ? &clip : nullptr;
  }
  auto Speed(const AnimationState &state) -> float {
    return state.speed > .0f ? state.speed : 1.f;
  }
  /// @brief How many of the application's ticks a state's clip lasts, rounded up so the last pose is reached.
  auto LengthTicks(const AnimationState &state, const AnimationClip &clip, const int tickRate) -> int {
    return std::max(1, static_cast<int>(std::ceil(clip.duration / clip.ticksPerSecond / Speed(state) * tickRate - 1e-4f)));
  }
  /// @brief Where in the clip, in the clip's own time, a number of ticks into the state falls.
  auto ClipTime(const AnimationState &state, const AnimationClip &clip, const int tickRate, const float ticks) -> float {
    return std::min(ticks / tickRate * Speed(state) * clip.ticksPerSecond, clip.duration);
  }
  auto RootChannel(const AnimationClip &clip, const std::string &rootNode) -> const AnimationChannel * {
    if (rootNode.empty())
      return nullptr;
    for (const auto &channel : clip.channels)
      if (channel.nodeName == rootNode && !channel.positions.empty())
        return &channel;
    return nullptr;
  }
} // namespace
auto StepAnimator(Animator &animator, const ModelAsset &model, const int tickRate) -> void {
  animator.events.clear();
  animator.rootDelta = {};
  if (animator.states.empty())
    return;
  if (animator.state < 0 || animator.state >= static_cast<int>(animator.states.size()))
    animator.Enter(0);
  auto &state = animator.states[animator.state];
  const auto *clip = StateClip(state, model);
  if (!clip)
    return;
  const auto length = LengthTicks(state, *clip, tickRate);
  const auto local = state.loop ? animator.stateTicks % length : animator.stateTicks;
  for (const auto &event : state.events)
    if (event.tick == local)
      animator.events.push_back(event.name);
  if (const auto *root = RootChannel(*clip, animator.rootNode); root && local < length) {
    // To the end of this tick or the end of the clip, whichever is first, and never round the
    // corner of a loop: the step from a clip's last pose back to its first is the clip starting
    // again, not the character travelling backwards the whole length of its stride.
    const auto from = SamplePosition(root->positions, ClipTime(state, *clip, tickRate, static_cast<float>(local)));
    const auto to = SamplePosition(root->positions, ClipTime(state, *clip, tickRate, static_cast<float>(local + 1)));
    animator.rootDelta = {to.x - from.x, .0f, to.z - from.z};
  }
  ++animator.stateTicks;
  if (state.loop || animator.stateTicks < length)
    return;
  if (const auto next = animator.FindState(state.next); next >= 0)
    animator.Enter(next);
  else
    animator.finished = true;
}
auto PoseAnimator(Animator &animator, const ModelAsset &model, const int tickRate, const float alpha) -> void {
  if (animator.state < 0 || animator.state >= static_cast<int>(animator.states.size()))
    return;
  auto &state = animator.states[animator.state];
  const auto *clip = StateClip(state, model);
  if (!clip)
    return;
  // One tick behind the newest and `alpha` of the way on from there: between the last two states
  // the rules have reached, which is the only stretch of time both ends of are known.
  const auto shown = std::max(.0f, static_cast<float>(animator.stateTicks) - 1.f + alpha);
  const auto length = static_cast<float>(LengthTicks(state, *clip, tickRate));
  const auto time = ClipTime(state, *clip, tickRate, state.loop ? std::fmod(shown, length) : shown);
  std::vector<BonePose> pose(model.nodes.size());
  for (const auto &channel : clip->channels) {
    if (channel.nodeIndex >= pose.size())
      continue;
    // From the bind pose, so a channel that keys only a rotation leaves the bone where the model
    // put it rather than at the origin.
    auto bone = BindPose(model.nodes[channel.nodeIndex]);
    if (!channel.positions.empty())
      bone.position = SamplePosition(channel.positions, time);
    if (!channel.rotations.empty())
      bone.rotation = SampleRotation(channel.rotations, time);
    if (!channel.scales.empty())
      bone.scale = SampleScale(channel.scales, time);
    if (!animator.rootNode.empty() && channel.nodeName == animator.rootNode && !channel.positions.empty()) {
      const auto start = SamplePosition(channel.positions, .0f);
      bone.position.x = start.x;
      bone.position.z = start.z;
    }
    pose[channel.nodeIndex] = bone;
  }
  const auto progress = animator.fadeTicks > 0 ? std::clamp(shown / static_cast<float>(animator.fadeTicks), .0f, 1.f) : 1.f;
  const auto weight = progress * progress * (3.f - 2.f * progress);
  if (weight < 1.f)
    for (size_t node = 0; node < pose.size() && node < animator.fadeFrom.size(); ++node) {
      if (!animator.fadeFrom[node].set)
        continue;
      // A bone the last clip moved and this one does not is faded back to where the model has it,
      // instead of being left wherever the last clip happened to leave it.
      pose[node] = Blend(animator.fadeFrom[node], pose[node].set ? pose[node] : BindPose(model.nodes[node]), weight);
    }
  animator.pose = std::move(pose);
}
AnimationSystem::AnimationSystem(Application &app)
  : System(std::in_place_type<AnimationSystem>, app) {}
void AnimationSystem::Start() {}
auto AnimationSystem::Tick() -> void {
  KUKI_PROFILE_SCOPE("Animation");
  app.ForEachEntity<Animator, Skeleton>([&](const EntityID id, Animator *animator, Skeleton *skeleton) {
    if (!animator || !skeleton || animator->states.empty())
      return;
    const auto *model = app.GetAsset<ModelAsset>(animator->modelAssetId);
    if (!model)
      return;
    StepAnimator(*animator, *model, app.TickRate());
    if (animator->rootDelta == glm::vec3(.0f))
      return;
    // The clip moves the root in the space of whatever the root hangs from, which an importer is
    // free to have scaled or stood on end -- a model in centimetres under a node that shrinks it
    // by a hundred is the usual case. Taken through that parent into the world, and back out
    // through the entity's own facing, it is the distance the entity should move along its own axes.
    for (size_t node = 0; node < model->nodes.size() && node < skeleton->nodeEntities.size(); ++node) {
      if (model->nodes[node].name != animator->rootNode)
        continue;
      const auto *parent = app.GetEntityComponent<Transform>(app.GetEntityParent(skeleton->nodeEntities[node]));
      const auto *self = app.GetEntityComponent<Transform>(id);
      if (!parent || !self)
        break;
      const auto travelled = glm::mat3(parent->world) * animator->rootDelta;
      const glm::mat3 facing{glm::normalize(glm::vec3(self->world[0])), glm::normalize(glm::vec3(self->world[1])), glm::normalize(glm::vec3(self->world[2]))};
      animator->rootDelta = glm::transpose(facing) * travelled;
      break;
    }
  });
}
void AnimationSystem::Update(const float deltaTime) {
  KUKI_PROFILE_SCOPE("Animation");
  app.ForEachEntity<Animator, Skeleton>([&](const EntityID, Animator *animator, Skeleton *skeleton) {
    if (!animator || !skeleton)
      return;
    auto modelAsset = app.GetAsset<ModelAsset>(animator->modelAssetId);
    if (!modelAsset)
      return;
    if (!animator->states.empty()) {
      // Stepped by `Tick`; all that is left for a frame to do is draw where that has got to.
      PoseAnimator(*animator, *modelAsset, app.TickRate(), app.TickAlpha());
      if (animator->state >= 0 && animator->state < static_cast<int>(animator->states.size()))
        animator->clipIndex = animator->states[animator->state].clipIndex;
      for (size_t node = 0; node < animator->pose.size() && node < skeleton->nodeEntities.size(); ++node) {
        const auto &bone = animator->pose[node];
        auto *transform = bone.set && skeleton->nodeEntities[node] ? app.GetEntityComponent<Transform>(skeleton->nodeEntities[node]) : nullptr;
        if (!transform)
          continue;
        transform->position = bone.position;
        transform->rotation = bone.rotation;
        transform->scale = bone.scale;
        app.MarkTransformDirty(skeleton->nodeEntities[node]);
      }
      return;
    }
    if (animator->clipIndex < 0 || animator->clipIndex >= static_cast<int>(modelAsset->animations.size()))
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
      app.MarkTransformDirty(boneEntityId);
    }
  });
}
void AnimationSystem::Shutdown() {}
} // namespace kuki
