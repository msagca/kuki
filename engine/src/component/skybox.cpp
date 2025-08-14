#include <component/skybox.hpp>
namespace kuki {
void Skybox::CopyTo(Skybox& other) const {
  other.skybox = skybox;
  other.irradiance = irradiance;
  other.prefilter = prefilter;
  other.brdf = brdf;
}
} // namespace kuki
