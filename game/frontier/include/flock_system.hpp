#pragma once
#include <flock.hpp>
#include <glm/ext/vector_float3.hpp>
#include <id.hpp>
#include <random.hpp>
#include <system.hpp>
#include <vector>
namespace kuki {
class Application;
} // namespace kuki
/// @brief Birds over the map: gulls along the shore and swallows over the land, in flocks that
/// wander from one place to another.
///
/// Each flock is a `kuki::Flock`, flown above the map's ground and carried a little by the wind,
/// and given somewhere new to make for every so often -- a gull flock a stretch of coast, a swallow
/// flock a field. Every bird is three entities, a body and two wings hinged to it, posed each frame:
/// the body along its heading and banked into its turns, the wings beating as hard as the flock
/// says the bird is working, and held out in a glide when it is not.
///
/// Presentation only, so it runs in `Update`, at the frame's own pace.
class FlockSystem final : public kuki::System {
public:
  explicit FlockSystem(kuki::Application &);
  auto Update(const float) -> void override;
private:
  struct Birds {
    kuki::Flock flock;
    bool gulls{};
    float scale{1.f};
    /// Seconds until it is given somewhere new to go.
    float wander{};
    std::vector<kuki::EntityID> bodies;
    std::vector<kuki::EntityID> left;
    std::vector<kuki::EntityID> right;
    std::vector<glm::vec3> heading;
    std::vector<float> roll;
  };
  std::vector<Birds> flocks;
  uint32_t generation{};
  kuki::Random random{0x5be0cd19137e2179ull};
  auto Release() -> void;
  auto Spawn() -> void;
  auto PickGoal(Birds &) -> void;
};
