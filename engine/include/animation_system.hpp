#pragma once
#include <system.hpp>
namespace kuki {
class KUKI_ENGINE_API AnimationSystem final : public System {
public:
  AnimationSystem();
  ~AnimationSystem();
  void Start() override;
  void Update(float) override;
  void LateUpdate(float) override;
  void Shutdown() override;
};
} // namespace kuki
