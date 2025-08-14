#include <component/component.hpp>
#include <component/script.hpp>
namespace kuki {
class Application;
IScript::IScript(Application& app, unsigned int entityId)
  : app(app), entityId(entityId) {}
} // namespace kuki
