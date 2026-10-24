#include <algorithm>
#include <application.hpp>
#include <command_line.hpp>
#include <network_system.hpp>
#include <spdlog/spdlog.h>
using namespace kuki;
namespace {
/// How big the lines in the corner are, how far apart, and how far in from the edge.
constexpr float STATUS_SIZE = 16.f;
constexpr float STATUS_SPACING = 22.f;
constexpr float STATUS_MARGIN = 24.f;
} // namespace
NetworkSystem::NetworkSystem(Application &app)
  : System(std::in_place_type<NetworkSystem>, app) {}
void NetworkSystem::Start() {
  // Here and not in the constructor. The application builds its systems as temporaries and moves
  // them into place, so an address taken at construction is one that is gone by the time it is used.
  app.SetNetwork(this);
  std::string error;
  const auto arguments = GetCommandLineArguments();
  options = ParseNetworkOptions(arguments, error);
  if (!error.empty())
    spdlog::error("[Network] {}", error);
  botAlone = std::ranges::find(arguments, "--bot") != arguments.end();
}
void NetworkSystem::Shutdown() {
  End();
  app.SetNetwork(nullptr);
}
auto NetworkSystem::SetProtocol(const uint32_t value) -> void {
  protocol = value;
}
auto NetworkSystem::SetSeat(const int player, Seat seat) -> void {
  if (player >= 0 && player < static_cast<int>(seats.size()))
    seats[player] = std::move(seat);
}
auto NetworkSystem::SetBot(Bot value) -> void {
  bot = std::move(value);
}
auto NetworkSystem::SetOptions(std::optional<NetworkOptions> value) -> void {
  options = std::move(value);
}
auto NetworkSystem::ShowStatus(const bool value) -> void {
  showStatus = value;
}
auto NetworkSystem::Begin(std::shared_ptr<RollbackGame> rules) -> void {
  End();
  NetplayConfig config;
  config.options = options;
  config.rules = std::move(rules);
  config.tickRate = app.TickRate();
  config.protocol = protocol;
  config.seats = seats;
  config.bot = bot;
  if (!options && botAlone && bot && !config.seats[0])
    config.seats[0] = [bot = bot](const int64_t tick) { return bot(0, tick); };
  netplay = std::make_unique<Netplay>(std::move(config));
}
auto NetworkSystem::End() -> void {
  if (!netplay)
    return;
  netplay.reset();
  // Back to the clock's own pace, which a match that ended while this machine was slowing would
  // otherwise leave it at.
  app.SetTickScale(1.);
}
auto NetworkSystem::GetNetplay() const -> const Netplay * {
  return netplay.get();
}
auto NetworkSystem::IsActive() const -> bool {
  return netplay != nullptr;
}
auto NetworkSystem::Tick() -> void {
  if (!netplay)
    return;
  netplay->Step(app.GetButtons(), app.TickTime());
  if (netplay->IsOnline())
    app.SetTickScale(netplay->GetTickScale());
}
void NetworkSystem::Update(const float) {
  if (!netplay || !showStatus)
    return;
  const auto lines = netplay->GetStatus();
  const auto alarm = netplay->GetPhase() == Netplay::Phase::Failed;
  auto &overlay = app.GetOverlay();
  // Up from the bottom corner, the last line lowest, where a game's own text at the top is not.
  for (size_t index = 0; index < lines.size(); ++index) {
    const auto up = static_cast<float>(lines.size() - 1 - index);
    const auto red = alarm || lines[index].starts_with("THE GAMES");
    overlay.DrawText(lines[index], {STATUS_MARGIN, -STATUS_MARGIN - STATUS_SPACING * up}, STATUS_SIZE, red ? glm::vec4(1.f, .35f, .3f, 1.f) : glm::vec4(.6f, 1.f, .7f, .9f), TextAnchor::BottomLeft);
  }
}
