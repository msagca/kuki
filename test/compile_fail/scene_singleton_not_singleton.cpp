// expect: that component is not a scene singleton
// A light read as though a scene had one: it may have many, and "any one of them" is no answer.
#include <application.hpp>
#include <light.hpp>
auto Read(kuki::Application &app) -> kuki::Light * {
  return app.GetSceneSingleton<kuki::Light>();
}
