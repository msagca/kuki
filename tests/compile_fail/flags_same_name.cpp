// expect: read a command-line flag of the same name
// A game's system that reads a flag the network system already reads.
#include <array>
#include <command_line.hpp>
#include <network_system.hpp>
#include <rendering_system.hpp>
#include <system_application.hpp>
class Lobby final : public kuki::System {
public:
  static constexpr std::array FLAGS{kuki::Flag{"--host", kuki::FlagKind::Value, "<name>", "the name to show in the lobby"}};
  explicit Lobby(kuki::Application &app)
    : System(std::in_place_type<Lobby>, app) {}
};
using Game = kuki::SystemApplication<kuki::EngineWorld, kuki::NetworkSystem, Lobby, kuki::RenderingSystem>;
static_assert(Game::FLAGS.size() > 0);
