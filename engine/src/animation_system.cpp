#include <algorithm>
#include <animation_system.hpp>
#include <animator.hpp>
#include <animator_controller.hpp>
#include <application.hpp>
#include <cmath>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/matrix.hpp>
#include <glm/gtc/quaternion.hpp>
#include <inverse_kinematics.hpp>
#include <model_asset.hpp>
#include <profiler.hpp>
#include <rig.hpp>
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
  /// @brief A clip named by where it is and what it is called, found the first time and kept by index.
  ///
  /// From the animator's own model unless a source is named, which `lookup` is asked for. The first
  /// time, the clip's channels are also matched to this model's nodes: by the index each channel
  /// carries for a clip of the model's own, and by name for one that came from elsewhere, where a
  /// channel's index counts nodes in a different file.
  auto ResolveClip(const AssetID source, const std::string &name, int &clipIndex, std::vector<int> &nodes, const ModelAsset &model, const ModelLookup &lookup) -> const AnimationClip * {
    const auto *from = &model;
    if (source) {
      from = lookup ? lookup(source) : nullptr;
      if (!from)
        return nullptr;
    }
    if (clipIndex < 0) {
      for (size_t i = 0; i < from->animations.size() && clipIndex < 0; ++i) {
        const std::string_view candidate = from->animations[i].name;
        const auto bar = candidate.rfind('|');
        if (name.empty() || candidate == name || (bar != std::string_view::npos && candidate.substr(bar + 1) == name))
          clipIndex = static_cast<int>(i);
      }
      nodes.clear();
    }
    if (clipIndex < 0 || clipIndex >= static_cast<int>(from->animations.size()))
      return nullptr;
    const auto &clip = from->animations[clipIndex];
    if (clip.duration <= .0f || clip.ticksPerSecond <= .0f)
      return nullptr;
    if (nodes.size() != clip.channels.size()) {
      nodes.assign(clip.channels.size(), -1);
      for (size_t channel = 0; channel < clip.channels.size(); ++channel) {
        if (from == &model) {
          nodes[channel] = static_cast<int>(clip.channels[channel].nodeIndex);
          continue;
        }
        for (size_t node = 0; node < model.nodes.size() && nodes[channel] < 0; ++node)
          if (model.nodes[node].name == clip.channels[channel].nodeName)
            nodes[channel] = static_cast<int>(node);
      }
    }
    return &clip;
  }
  auto StateClip(const AnimationState &state, StateBinding &binding, const ModelAsset &model, const ModelLookup &lookup) -> const AnimationClip * {
    return ResolveClip(state.source, state.clip, binding.clip.clipIndex, binding.clip.nodes, model, lookup);
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
  auto Seconds(const AnimationClip &clip) -> float {
    return clip.duration / clip.ticksPerSecond;
  }
  /// @brief Writes a clip's pose at a time into the slots of the nodes its channels move.
  auto SampleInto(std::vector<BonePose> &pose, const AnimationClip &clip, const std::vector<int> &nodes, const float time, const ModelAsset &model, const std::string &rootNode) -> void {
    for (size_t index = 0; index < clip.channels.size() && index < nodes.size(); ++index) {
      const auto &channel = clip.channels[index];
      const auto node = nodes[index];
      if (node < 0 || node >= static_cast<int>(pose.size()))
        continue;
      // From the bind pose, so a channel that keys only a rotation leaves the bone where the model
      // put it rather than at the origin.
      auto bone = BindPose(model.nodes[node]);
      if (!channel.positions.empty())
        bone.position = SamplePosition(channel.positions, time);
      if (!channel.rotations.empty())
        bone.rotation = SampleRotation(channel.rotations, time);
      if (!channel.scales.empty())
        bone.scale = SampleScale(channel.scales, time);
      if (!rootNode.empty() && channel.nodeName == rootNode && !channel.positions.empty()) {
        const auto start = SamplePosition(channel.positions, .0f);
        bone.position.x = start.x;
        bone.position.z = start.z;
      }
      pose[node] = bone;
    }
  }
  /// @brief The two clips of a blend that the state's value lies between, and how far towards the second.
  ///
  /// The nearest at or below the value and the nearest at or above it. Outside the range of them
  /// all, both are the one at that end, which holds the blend there rather than carrying on past it.
  struct Bracket {
    const AnimationClip *low{};
    const AnimationClip *high{};
    const std::vector<int> *lowNodes{};
    const std::vector<int> *highNodes{};
    float weight{};
  };
  auto BlendBracket(const AnimationState &state, StateBinding &binding, const float value, const ModelAsset &model, const ModelLookup &lookup) -> Bracket {
    Bracket bracket;
    auto below = -1;
    auto above = -1;
    for (size_t index = 0; index < state.blend.size() && index < binding.blend.size(); ++index) {
      const auto &entry = state.blend[index];
      const auto *clip = ResolveClip(entry.source, entry.clip, binding.blend[index].clipIndex, binding.blend[index].nodes, model, lookup);
      if (!clip)
        continue;
      if (entry.at <= value && (below < 0 || entry.at >= state.blend[below].at)) {
        below = static_cast<int>(index);
        bracket.low = clip;
      }
      if (entry.at >= value && (above < 0 || entry.at <= state.blend[above].at)) {
        above = static_cast<int>(index);
        bracket.high = clip;
      }
    }
    if (below < 0) {
      below = above;
      bracket.low = bracket.high;
    }
    if (above < 0) {
      above = below;
      bracket.high = bracket.low;
    }
    if (below < 0)
      return {};
    bracket.lowNodes = &binding.blend[below].nodes;
    bracket.highNodes = &binding.blend[above].nodes;
    const auto low = state.blend[below].at;
    const auto high = state.blend[above].at;
    bracket.weight = high > low ? (value - low) / (high - low) : .0f;
    return bracket;
  }
  /// @brief How far a clip carries the root along the ground between two points of its cycle.
  ///
  /// The points are fractions of the clip, and the second may be past the end: the cycle has then
  /// come round, and the travel is what was left of this time through added to the start of the
  /// next. That is the difference from a clip played on its own, which stops counting at its end --
  /// a blend is only ever a loop, and a stride that lost its last step every cycle would fall short.
  auto RootTravel(const AnimationClip &clip, const std::string &rootNode, const float from, const float to) -> glm::vec3 {
    const auto *root = RootChannel(clip, rootNode);
    if (!root)
      return {};
    const auto at = [&](const float phase) { return SamplePosition(root->positions, phase * clip.duration); };
    const auto travelled = to <= 1.f ? at(to) - at(from) : (at(1.f) - at(from)) + (at(to - 1.f) - at(.0f));
    return {travelled.x, .0f, travelled.z};
  }
} // namespace
auto StepAnimator(Animator &animator, const AnimatorController &controller, const ModelAsset &model, const int tickRate, const ModelLookup &lookup) -> void {
  animator.fired = 0;
  animator.firedState = -1;
  animator.rootDelta = {};
  if (controller.states.empty())
    return;
  if (animator.state < 0 || animator.state >= static_cast<int>(controller.states.size()))
    Enter(animator, controller, 0);
  const auto &state = controller.states[animator.state];
  auto &binding = controller.Bind(model)[animator.state];
  const auto &rootNode = controller.rootNode;
  // Which of the state's events fall on this tick, as bits of a mask rather than names in a list:
  // the names are the controller's, and a tick that fires one allocates nothing.
  const auto Fire = [&](const int tick) {
    for (size_t event = 0; event < state.events.size() && event < 32; ++event)
      if (state.events[event].tick == tick) {
        animator.fired |= 1u << event;
        animator.firedState = animator.state;
      }
  };
  if (!state.blend.empty()) {
    const auto bracket = BlendBracket(state, binding, animator.blend, model, lookup);
    if (!bracket.low)
      return;
    Fire(animator.stateTicks);
    // One cycle of the blend lasts as long as the two clips' cycles mixed by the same weight as
    // their poses, and both are played at that one rate. So a walk and a run stay in step with one
    // another -- left foot down in both at the same fraction of the cycle -- however unlike their
    // own lengths are, which is the whole of what keeps a blend of two gaits from sliding.
    const auto seconds = glm::mix(Seconds(*bracket.low), Seconds(*bracket.high), bracket.weight) / Speed(state);
    const auto from = animator.phase;
    auto to = from + 1.f / (static_cast<float>(tickRate) * seconds);
    animator.rootDelta = glm::mix(RootTravel(*bracket.low, rootNode, from, to), RootTravel(*bracket.high, rootNode, from, to), bracket.weight);
    animator.previousPhase = from;
    ++animator.stateTicks;
    if (to >= 1.f) {
      if (state.loop) {
        // Both ends moved back by a cycle together, so that drawing between them still runs
        // forwards across the join.
        to -= 1.f;
        animator.previousPhase -= 1.f;
      } else {
        to = 1.f;
        if (const auto next = controller.FindState(state.next); next >= 0) {
          Enter(animator, controller, next);
          return;
        }
        animator.finished = true;
      }
    }
    animator.phase = to;
    return;
  }
  const auto *clip = StateClip(state, binding, model, lookup);
  if (!clip)
    return;
  const auto length = LengthTicks(state, *clip, tickRate);
  const auto local = state.loop ? animator.stateTicks % length : animator.stateTicks;
  Fire(local);
  if (const auto *root = RootChannel(*clip, rootNode); root && local < length) {
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
  if (const auto next = controller.FindState(state.next); next >= 0)
    Enter(animator, controller, next);
  else
    animator.finished = true;
}
auto PoseAnimator(Animator &animator, Rig &rig, const AnimatorController &controller, const ModelAsset &model, const int tickRate, const float alpha, const ModelLookup &lookup) -> void {
  if (animator.state < 0 || animator.state >= static_cast<int>(controller.states.size()))
    return;
  const auto &state = controller.states[animator.state];
  auto &binding = controller.Bind(model)[animator.state];
  const auto &rootNode = controller.rootNode;
  // A state entered since the last frame fades out of the pose that frame showed, which is still
  // the rig's. The first time there is none, and so nothing to fade out of.
  if (rig.entries != animator.entries) {
    rig.entries = animator.entries;
    rig.fadeFrom = rig.pose;
  }
  // One tick behind the newest and `alpha` of the way on from there: between the last two states
  // the rules have reached, which is the only stretch of time both ends of are known.
  const auto shown = std::max(.0f, static_cast<float>(animator.stateTicks) - 1.f + alpha);
  // Written over in place, frame after frame, so that drawing a pose allocates nothing once the
  // buffers have grown to the model's size.
  auto &pose = rig.pose;
  if (!state.blend.empty()) {
    const auto bracket = BlendBracket(state, binding, animator.blend, model, lookup);
    if (!bracket.low)
      return;
    auto phase = animator.previousPhase + alpha * (animator.phase - animator.previousPhase);
    phase -= std::floor(phase);
    pose.assign(model.nodes.size(), BonePose{});
    SampleInto(pose, *bracket.low, *bracket.lowNodes, phase * bracket.low->duration, model, rootNode);
    if (bracket.high != bracket.low && bracket.weight > .0f) {
      auto &other = rig.scratch;
      other.assign(model.nodes.size(), BonePose{});
      SampleInto(other, *bracket.high, *bracket.highNodes, phase * bracket.high->duration, model, rootNode);
      for (size_t node = 0; node < pose.size(); ++node) {
        // A bone only one of the two moves is blended with where the model has it, which is where
        // the other leaves it.
        if (pose[node].set || other[node].set)
          pose[node] = Blend(pose[node].set ? pose[node] : BindPose(model.nodes[node]), other[node].set ? other[node] : BindPose(model.nodes[node]), bracket.weight);
      }
    }
  } else {
    const auto *clip = StateClip(state, binding, model, lookup);
    if (!clip)
      return;
    const auto length = static_cast<float>(LengthTicks(state, *clip, tickRate));
    pose.assign(model.nodes.size(), BonePose{});
    SampleInto(pose, *clip, binding.clip.nodes, ClipTime(state, *clip, tickRate, state.loop ? std::fmod(shown, length) : shown), model, rootNode);
  }
  const auto progress = animator.fadeTicks > 0 ? std::clamp(shown / static_cast<float>(animator.fadeTicks), .0f, 1.f) : 1.f;
  const auto weight = progress * progress * (3.f - 2.f * progress);
  if (weight < 1.f)
    for (size_t node = 0; node < pose.size() && node < rig.fadeFrom.size(); ++node) {
      if (!rig.fadeFrom[node].set)
        continue;
      // A bone the last clip moved and this one does not is faded back to where the model has it,
      // instead of being left wherever the last clip happened to leave it.
      pose[node] = Blend(rig.fadeFrom[node], pose[node].set ? pose[node] : BindPose(model.nodes[node]), weight);
    }
}
AnimationSystem::AnimationSystem(Application &app)
  : System(std::in_place_type<AnimationSystem>, app) {}
void AnimationSystem::Start() {}
auto AnimationSystem::Tick() -> void {
  KUKI_PROFILE_SCOPE("Animation");
  app.ForEachEntity<Animator, Skeleton>([&](const EntityID id, Animator *animator, Skeleton *) {
    const auto *rig = app.GetRig(id);
    const auto *controller = animator ? app.GetAsset<AnimatorController>(animator->controller) : nullptr;
    if (!rig || !controller || controller->states.empty())
      return;
    const auto *model = app.GetAsset<ModelAsset>(animator->modelAssetId);
    if (!model)
      return;
    StepAnimator(*animator, *controller, *model, app.TickRate(), [this](const AssetID id) -> const ModelAsset * { return app.GetAsset<ModelAsset>(id); });
    if (animator->rootDelta == glm::vec3(.0f))
      return;
    // The clip moves the root in the space of whatever the root hangs from, which an importer is
    // free to have scaled or stood on end -- a model in centimetres under a node that shrinks it
    // by a hundred is the usual case. Taken through that parent into the world, and back out
    // through the entity's own facing, it is the distance the entity should move along its own axes.
    for (size_t node = 0; node < model->nodes.size() && node < rig->nodes.size(); ++node) {
      if (model->nodes[node].name != controller->rootNode)
        continue;
      const auto *parent = app.GetEntityComponent<WorldMatrix>(app.GetEntityParent(rig->nodes[node]));
      const auto *self = app.GetEntityComponent<WorldMatrix>(id);
      if (!parent || !self)
        break;
      const auto travelled = glm::mat3(parent->matrix) * animator->rootDelta;
      const glm::mat3 facing{glm::normalize(glm::vec3(self->matrix[0])), glm::normalize(glm::vec3(self->matrix[1])), glm::normalize(glm::vec3(self->matrix[2]))};
      animator->rootDelta = glm::transpose(facing) * travelled;
      break;
    }
  });
}
void AnimationSystem::Update(const float deltaTime) {
  KUKI_PROFILE_SCOPE("Animation");
  const auto stopped = app.GetPlayState() == PlayState::Stopped;
  const ModelLookup lookup = [this](const AssetID id) -> const ModelAsset * { return app.GetAsset<ModelAsset>(id); };
  preview.SetRate(app.TickRate());
  const auto previewTicks = stopped ? preview.Advance(deltaTime) : 0;
  app.ForEachEntity<Animator, Skeleton>([&](const EntityID id, Animator *animator, Skeleton *) {
    auto *rig = app.GetRig(id);
    if (!animator || !rig)
      return;
    auto modelAsset = app.GetAsset<ModelAsset>(animator->modelAssetId);
    if (!modelAsset)
      return;
    if (const auto *controller = app.GetAsset<AnimatorController>(animator->controller); controller && !controller->states.empty()) {
      // Stepped by `Tick` while the game plays, and from here while it is stopped -- see `preview`.
      // Either way all that is left for the frame to do is draw where that has got to. A stopped
      // animator that is not playing is drawn at the tick it stands on rather than between two.
      const auto previewing = stopped && animator->playing;
      for (auto tick = 0; previewing && tick < previewTicks; ++tick)
        StepAnimator(*animator, *controller, *modelAsset, app.TickRate(), lookup);
      PoseAnimator(*animator, *rig, *controller, *modelAsset, app.TickRate(), stopped ? (previewing ? preview.Alpha() : 1.f) : app.TickAlpha(), lookup);
      // Which clip is showing, for the editor to name. A blend has none of its own to name.
      if (animator->state >= 0 && animator->state < static_cast<int>(controller->states.size()))
        animator->clipIndex = controller->Bind(*modelAsset)[animator->state].clip.clipIndex;
      for (size_t node = 0; node < rig->pose.size() && node < rig->nodes.size(); ++node) {
        const auto &bone = rig->pose[node];
        auto *transform = bone.set && rig->nodes[node] ? app.GetEntityComponent<Transform>(rig->nodes[node]) : nullptr;
        if (!transform)
          continue;
        transform->position = bone.position;
        transform->rotation = bone.rotation;
        transform->scale = bone.scale;
        app.MarkTransformDirty(rig->nodes[node]);
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
      if (channel.nodeIndex >= rig->nodes.size())
        continue;
      const auto boneEntityId = rig->nodes[channel.nodeIndex];
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
auto AnimationSystem::LateUpdate(const float) -> void {
  KUKI_PROFILE_SCOPE("Inverse Kinematics");
  app.ForEachEntity<Animator, Skeleton>([&](const EntityID id, Animator *animator, Skeleton *) {
    auto *rig = app.GetRig(id);
    // Not only where there are goals: one removed since the last frame leaves bones to be put back.
    if (!animator || !rig || (animator->ik.empty() && rig->held.empty()))
      return;
    if (const auto *model = app.GetAsset<ModelAsset>(animator->modelAssetId); model)
      ApplyInverseKinematics(app, id, *animator, *rig, *model);
  });
}
void AnimationSystem::Shutdown() {}
} // namespace kuki
