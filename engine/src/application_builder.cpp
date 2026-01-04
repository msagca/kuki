#include <application.hpp>
#include <application_builder.hpp>
#include <memory>
namespace kuki {
auto ApplicationBuilder::Build() -> std::unique_ptr<Application> {
  return std::move(app);
}
} // namespace kuki
