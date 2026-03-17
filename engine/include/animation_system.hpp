#pragma once
#include <system.hpp>
namespace kuki {
class KUKI_ENGINE_API AnimationSystem final : public System {
public:
  AnimationSystem();
  ~AnimationSystem();
  void Awake() override;
  void Start() override;
  void Update(const float) override;
  void Shutdown() override;
};
} // namespace kuki
