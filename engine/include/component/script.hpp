#pragma once
#include <application.hpp>
#include "component.hpp"
#include <kuki_export.h>
namespace kuki {
struct KUKI_API Script : IComponent {
  int id{};
  const std::string GetName() const override;
  std::vector<kuki::Property> GetProperties() const override;
  void SetProperty(kuki::Property) override;
};
class KUKI_API IScript : IComponent {
protected:
  Application& app;
  unsigned int entityId;
public:
  IScript(Application&, unsigned int);
  virtual ~IScript() = default;
  virtual void Update(float) = 0;
};
} // namespace kuki
