#include <algorithm>
#include <animator.hpp>
#include <application.hpp>
#include <cmath>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/trigonometric.hpp>
#include <inverse_kinematics.hpp>
#include <model_asset.hpp>
#include <skeleton.hpp>
#include <transform.hpp>
#include <vector>
namespace kuki {
namespace {
  constexpr float EPSILON = 1e-6f;
  const glm::quat IDENTITY{1.f, .0f, .0f, .0f};
  /// @brief What is left of `v` once the part of it along the unit `axis` is taken away.
  auto Across(const glm::vec3 &v, const glm::vec3 &axis) -> glm::vec3 {
    return v - axis * glm::dot(v, axis);
  }
  /// @brief Some direction square to `v`, for when nothing says which.
  auto AnyAcross(const glm::vec3 &v) -> glm::vec3 {
    return glm::normalize(glm::cross(v, std::abs(v.x) < .9f ? glm::vec3(1.f, .0f, .0f) : glm::vec3(.0f, 1.f, .0f)));
  }
  auto Local(const Transform &transform) -> glm::mat4 {
    return glm::translate(glm::mat4(1.f), transform.position) * glm::toMat4(transform.rotation) * glm::scale(glm::mat4(1.f), transform.scale);
  }
  /// @brief The turn part of a matrix that may also scale.
  ///
  /// Scaled the same along every axis, as anything with bones in it is, or the turn is only roughly
  /// this. A skeleton under a node that squeezes it one way would have bones whose lengths change as
  /// they turn, and no solver that only turns them is the answer to that.
  auto RotationOf(const glm::mat4 &matrix) -> glm::quat {
    glm::mat3 axes(matrix);
    for (auto column = 0; column < 3; ++column) {
      const auto length = glm::length(axes[column]);
      if (length > EPSILON)
        axes[column] /= length;
    }
    return glm::normalize(glm::quat_cast(axes));
  }
  auto PositionOf(const glm::mat4 &matrix) -> glm::vec3 {
    return glm::vec3(matrix[3]);
  }
  /// @brief Solves one animator's goals. Kept together because every step needs the same four things.
  struct Solver {
    Application &app;
    const EntityID self;
    Animator &animator;
    const Skeleton &skeleton;
    const ModelAsset &model;
    /// @brief Where an entity is in the world as its transforms stand now.
    ///
    /// Up through every parent, from what each one holds and not from its world matrix. Those were
    /// last made before the pose was written this frame and before the game moved anything, and a
    /// foot put down by them would be put down where the character was a frame ago.
    auto WorldOf(const EntityID id) const -> glm::mat4 {
      glm::mat4 world(1.f);
      for (auto at = id; at; at = app.GetEntityParent(at)) {
        const auto *transform = app.GetEntityComponent<Transform>(at);
        if (!transform)
          break;
        world = Local(*transform) * world;
      }
      return world;
    }
    auto Node(const std::string &name) const -> int {
      for (size_t node = 0; node < model.nodes.size(); ++node)
        if (model.nodes[node].name == name)
          return static_cast<int>(node);
      return -1;
    }
    auto EntityOf(const int node) const -> EntityID {
      return node >= 0 && node < static_cast<int>(skeleton.nodeEntities.size()) ? skeleton.nodeEntities[node] : EntityID::Invalid;
    }
    auto Point(const IKGoal &goal) const -> glm::vec3 {
      if (goal.target && app.GetEntityComponent<Transform>(goal.target))
        return PositionOf(WorldOf(goal.target) * glm::translate(glm::mat4(1.f), goal.offset));
      return goal.offset;
    }
    /// @brief Gives a bone a new rotation of its own, remembering the one the pose gave it first.
    auto Set(const EntityID bone, const glm::quat &rotation) -> void {
      auto *transform = app.GetEntityComponent<Transform>(bone);
      if (!transform)
        return;
      if (std::ranges::none_of(animator.ikHeld, [&](const IKHeld &held) { return held.bone == bone; }))
        animator.ikHeld.push_back({bone, transform->rotation, transform->rotation});
      transform->rotation = rotation;
      app.MarkTransformDirty(bone);
    }
    /// @brief Turns a bone by a turn given in the world, about its own joint.
    ///
    /// The bone's rotation is its parent's turn undone, then its own: so the world turn is moved into
    /// the parent's frame and put in front of the bone's own rotation there.
    auto TurnBy(const EntityID bone, const glm::quat &turn) -> void {
      const auto *transform = app.GetEntityComponent<Transform>(bone);
      if (!transform)
        return;
      const auto parent = RotationOf(WorldOf(app.GetEntityParent(bone)));
      Set(bone, glm::normalize(glm::inverse(parent) * turn * parent * transform->rotation));
    }
    /// @brief How far down the skeleton the highest bone a goal turns is: the number of nodes above it.
    auto Depth(const IKGoal &goal) const -> int {
      auto top = Node(goal.bone);
      const auto above = goal.kind == IKKind::Aim ? std::max(1, goal.chain) - 1 : 2;
      for (auto step = 0; step < above && top >= 0 && model.nodes[top].parent >= 0; ++step)
        top = model.nodes[top].parent;
      auto depth = 0;
      for (auto at = top; at >= 0; at = model.nodes[at].parent)
        ++depth;
      return depth;
    }
    auto Reach(const IKGoal &goal, const float weight) -> void {
      const auto end = Node(goal.bone);
      const auto mid = end >= 0 ? model.nodes[end].parent : -1;
      const auto top = mid >= 0 ? model.nodes[mid].parent : -1;
      const auto c = EntityOf(end);
      const auto b = EntityOf(mid);
      const auto a = EntityOf(top);
      if (!a || !b || !c)
        return;
      const auto endWorld = WorldOf(c);
      const auto bend = glm::length(goal.bend) > EPSILON ? RotationOf(WorldOf(self)) * goal.bend : glm::vec3(.0f);
      const auto turn = SolveTwoBone(PositionOf(WorldOf(a)), PositionOf(WorldOf(b)), PositionOf(endWorld), Point(goal), bend);
      // The middle joint first, while the limb is where the turn was worked out for, and then the
      // top, which carries the middle joint's turn along with everything else below it.
      TurnBy(b, glm::slerp(IDENTITY, turn.mid, weight));
      TurnBy(a, glm::slerp(IDENTITY, turn.root, weight));
      if (goal.holdRotation)
        Set(c, glm::normalize(glm::inverse(RotationOf(WorldOf(app.GetEntityParent(c)))) * RotationOf(endWorld)));
    }
    auto Aim(const IKGoal &goal, const float weight) -> void {
      const auto node = Node(goal.bone);
      const auto bone = EntityOf(node);
      if (!bone || glm::length(goal.axis) < EPSILON)
        return;
      // The axis is given in the character's own terms, and is wanted in the bone's: taken through
      // every node between the two as the model was built. A bone lies however its rig had it --
      // along its own length, usually, which for a head is up -- and nobody should have to know that
      // to make one look at something.
      auto bind = IDENTITY;
      for (auto at = node; at >= 0 && EntityOf(at) != self; at = model.nodes[at].parent)
        bind = RotationOf(model.nodes[at].transform) * bind;
      const auto axis = glm::inverse(bind) * glm::normalize(goal.axis);
      std::vector<EntityID> chain;
      for (auto at = node; at >= 0 && static_cast<int>(chain.size()) < std::max(1, goal.chain); at = model.nodes[at].parent) {
        const auto entity = EntityOf(at);
        if (entity == self)
          break;
        if (entity)
          chain.push_back(entity);
      }
      const auto start = WorldOf(bone);
      const auto toPoint = Point(goal) - PositionOf(start);
      if (glm::length(toPoint) < EPSILON)
        return;
      const auto facing = RotationOf(start) * axis;
      const auto wanted = glm::slerp(IDENTITY, SolveAim(facing, toPoint, glm::radians(goal.limit)), weight) * facing;
      // From the top of the chain down, each taking an even share of what is left to turn: a third,
      // then half of the rest, then all of what remains. Asked afresh at every bone, since each
      // turn above moves the one below.
      for (auto share = static_cast<int>(chain.size()); share > 0; --share) {
        const auto now = RotationOf(WorldOf(bone)) * axis;
        TurnBy(chain[share - 1], glm::slerp(IDENTITY, glm::rotation(glm::normalize(now), glm::normalize(wanted)), 1.f / static_cast<float>(share)));
      }
    }
  };
} // namespace
auto SolveTwoBone(const glm::vec3 &a, const glm::vec3 &b, const glm::vec3 &c, const glm::vec3 &target, const glm::vec3 &bend) -> TwoBoneTurn {
  const auto upper = glm::length(b - a);
  const auto lower = glm::length(c - b);
  const auto toTarget = target - a;
  const auto distance = glm::length(toTarget);
  if (upper < EPSILON || lower < EPSILON || distance < EPSILON)
    return {};
  const auto span = upper + lower;
  const auto reach = std::clamp(distance, std::abs(upper - lower) + span * 1e-4f, span * (1.f - 1e-4f));
  // The middle joint, opened or closed by the law of cosines until its two ends are `reach` apart.
  // Turned about the line square to both bones: a positive turn about that line brings the lower
  // bone towards the upper, which closes the joint.
  const auto opened = std::acos(std::clamp(glm::dot(glm::normalize(a - b), glm::normalize(c - b)), -1.f, 1.f));
  const auto wanted = std::acos(std::clamp((upper * upper + lower * lower - reach * reach) / (2.f * upper * lower), -1.f, 1.f));
  auto hinge = glm::cross(c - b, a - b);
  if (glm::length(hinge) < 1e-4f * upper * lower) {
    // Straight, or folded flat, and there is no plane to bend in. The one that puts the joint out
    // towards `bend` is made up: as though the limb were already bent a little that way.
    const auto along = glm::length(c - a) > EPSILON ? glm::normalize(c - a) : glm::normalize(c - b);
    auto side = Across(bend, along);
    side = glm::length(side) > EPSILON ? glm::normalize(side) : AnyAcross(along);
    hinge = glm::cross(c - b, -side);
  }
  const auto mid = glm::angleAxis(opened - wanted, glm::normalize(hinge));
  const auto bent = b + mid * (c - b);
  // The limb swung at the top joint so that the line to its end is the line to the target.
  const auto toward = toTarget / distance;
  auto root = glm::rotation(glm::normalize(bent - a), toward);
  // And turned about that line until the middle joint is on the side `bend` asks for. The end
  // does not move, being on the line.
  if (glm::length(bend) > EPSILON) {
    const auto joint = Across(root * (b - a), toward);
    const auto side = Across(bend, toward);
    if (glm::length(joint) > EPSILON * upper && glm::length(side) > EPSILON) {
      const auto from = glm::normalize(joint);
      const auto to = glm::normalize(side);
      root = glm::angleAxis(std::atan2(glm::dot(glm::cross(from, to), toward), glm::dot(from, to)), toward) * root;
    }
  }
  return {glm::normalize(root), mid};
}
auto SolveAim(const glm::vec3 &direction, const glm::vec3 &wanted, const float limit) -> glm::quat {
  if (glm::length(direction) < EPSILON || glm::length(wanted) < EPSILON)
    return IDENTITY;
  const auto full = glm::rotation(glm::normalize(direction), glm::normalize(wanted));
  const auto angle = glm::angle(full);
  if (angle <= limit || angle < EPSILON)
    return full;
  return glm::slerp(IDENTITY, full, std::max(.0f, limit) / angle);
}
auto UndoInverseKinematics(Application &app, Animator &animator) -> void {
  for (const auto &held : animator.ikHeld)
    if (auto *transform = app.GetEntityComponent<Transform>(held.bone); transform && transform->rotation == held.solved) {
      transform->rotation = held.posed;
      app.MarkTransformDirty(held.bone);
    }
  animator.ikHeld.clear();
}
auto ApplyInverseKinematics(Application &app, const EntityID self, Animator &animator, const Skeleton &skeleton, const ModelAsset &model) -> void {
  // What the goals did last frame is undone first wherever nothing has undone it since. See
  // `IKHeld` for why this matters.
  UndoInverseKinematics(app, animator);
  // From the top of the skeleton down rather than in the order they are listed. A goal turns bones
  // and everything hanging from them, so one solved after another that turns a bone further down
  // its tree would carry that one's answer away with it: a head turned by the chest, after a hand
  // was put on a point, takes the hand off the point. Solved the other way round, each one sees
  // what those above it have done and finishes where it meant to. Goals at the same depth do not
  // reach one another, and keep the order they were listed in.
  Solver solver{app, self, animator, skeleton, model};
  std::vector<std::pair<int, const IKGoal *>> order;
  for (const auto &goal : animator.ik)
    order.emplace_back(solver.Depth(goal), &goal);
  std::ranges::stable_sort(order, {}, &std::pair<int, const IKGoal *>::first);
  for (const auto &[depth, pointer] : order) {
    const auto &goal = *pointer;
    const auto weight = std::clamp(goal.weight, .0f, 1.f);
    if (weight <= .0f)
      continue;
    if (goal.kind == IKKind::Aim)
      solver.Aim(goal, weight);
    else
      solver.Reach(goal, weight);
  }
  for (auto &held : animator.ikHeld)
    if (const auto *transform = app.GetEntityComponent<Transform>(held.bone); transform)
      held.solved = transform->rotation;
}
} // namespace kuki
