#include <memory>
#include <render_graph_builder.hpp>
#include <target_description.hpp>
namespace kuki {
auto RenderGraphBuilder::AddInput(std::string name) -> RenderGraphBuilder & {
  if (renderGraph)
    renderGraph->AddInput(std::move(name));
  return *this;
}
auto RenderGraphBuilder::AddOutput(std::string name) -> RenderGraphBuilder & {
  if (renderGraph)
    renderGraph->AddOutput(std::move(name), descLast);
  return *this;
}
auto RenderGraphBuilder::AddOutput(std::string name, TargetDescription desc) -> RenderGraphBuilder & {
  descLast = desc;
  if (renderGraph)
    renderGraph->AddOutput(std::move(name), desc);
  return *this;
}
auto RenderGraphBuilder::BeginGraph() -> RenderGraphBuilder & {
  renderGraph = std::make_unique<RenderGraph>();
  return *this;
}
auto RenderGraphBuilder::EndGraph() -> std::unique_ptr<RenderGraph> {
  return std::move(renderGraph);
}
auto RenderGraphBuilder::EndPass() -> RenderGraphBuilder & {
  if (renderGraph)
    renderGraph->EndPass();
  return *this;
}
} // namespace kuki
