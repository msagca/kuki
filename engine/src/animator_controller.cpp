#include <animator_controller.hpp>
#include <model_asset.hpp>
#include <string_view>
#include <utility>
#include <vector>
namespace kuki {
AnimatorController::AnimatorController(AssetID id)
  : Asset(std::in_place_type<AnimatorController>, id) {}
auto AnimatorController::FindState(const std::string_view name) const -> int {
  if (name.empty())
    return -1;
  for (size_t i = 0; i < states.size(); ++i)
    if (states[i].name == name)
      return static_cast<int>(i);
  return -1;
}
auto AnimatorController::Bind(const ModelAsset &model) const -> std::vector<StateBinding> & {
  auto &binding = bindings[model.id];
  if (binding.size() != states.size())
    binding.resize(states.size());
  for (size_t state = 0; state < states.size(); ++state)
    if (binding[state].blend.size() != states[state].blend.size())
      binding[state].blend.resize(states[state].blend.size());
  return binding;
}
auto AnimatorController::Unbind() const -> void {
  bindings.clear();
}
} // namespace kuki
