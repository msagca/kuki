#include <algorithm>
#include <cstdlib>
#include <format>
#include <network_options.hpp>
using namespace kuki;
namespace {
auto ToNumber(const std::string &text, double &out) -> bool {
  char *end = nullptr;
  out = std::strtod(text.c_str(), &end);
  return !text.empty() && end == text.c_str() + text.size();
}
} // namespace
auto kuki::ParseNetworkOptions(const std::span<const std::string> arguments, std::string &error) -> std::optional<NetworkOptions> {
  NetworkOptions options;
  auto hosting = false, joining = false;
  for (size_t index = 0; index < arguments.size(); ++index) {
    const auto &argument = arguments[index];
    const auto value = [&](double &out) {
      if (index + 1 < arguments.size() && ToNumber(arguments[index + 1], out)) {
        ++index;
        return true;
      }
      error = std::format("{} needs a number", argument);
      return false;
    };
    double number{};
    if (argument == HOST_FLAG.name) {
      if (!value(number))
        return std::nullopt;
      options.host = hosting = true;
      options.port = static_cast<uint16_t>(std::clamp(number, 0., 65535.));
    } else if (argument == JOIN_FLAG.name) {
      if (index + 1 >= arguments.size()) {
        error = "--join needs an address and a port, as host:port";
        return std::nullopt;
      }
      const auto &target = arguments[++index];
      const auto colon = target.rfind(':');
      if (colon == std::string::npos || colon == 0 || !ToNumber(target.substr(colon + 1), number)) {
        error = std::format("--join {} is not host:port", target);
        return std::nullopt;
      }
      options.address = target.substr(0, colon);
      options.port = static_cast<uint16_t>(std::clamp(number, 0., 65535.));
      joining = true;
    } else if (argument == LAG_FLAG.name) {
      if (!value(number))
        return std::nullopt;
      options.lag = std::max(0., number) / 1000.;
    } else if (argument == JITTER_FLAG.name) {
      if (!value(number))
        return std::nullopt;
      options.jitter = std::max(0., number) / 1000.;
    } else if (argument == LOSS_FLAG.name) {
      if (!value(number))
        return std::nullopt;
      options.loss = static_cast<float>(std::clamp(number, 0., 100.) / 100.);
    } else if (argument == DELAY_FLAG.name) {
      if (!value(number))
        return std::nullopt;
      options.inputDelay = static_cast<int>(number);
    } else if (argument == PREDICTION_FLAG.name) {
      if (!value(number))
        return std::nullopt;
      options.maxPrediction = static_cast<int>(number);
    } else if (argument == BOT_FLAG.name)
      options.bot = true;
  }
  if (hosting && joining) {
    error = "--host and --join cannot both be given";
    return std::nullopt;
  }
  if (!hosting && !joining)
    return std::nullopt;
  return options;
}
