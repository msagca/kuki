// `kuki`: starts a game in a repository of its own and looks after it -- the engine it is pinned to,
// its build and its run. Installed with the engine, and on the PATH, so it works before there is
// anything to work in.
//
// It carries no template of its own. `kuki new` adds the engine as a submodule first and copies
// the template out of that checkout, so a new game always starts from the template of the engine
// it is built against -- which kuki's own build keeps compiling -- and never from a copy that went
// stale inside this executable.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <game_name.hpp>
#include <initializer_list>
#include <optional>
#include <process.hpp>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>
namespace fs = std::filesystem;
using namespace kuki::cli;
namespace {
/// Where a new game takes the engine from, unless `--engine` says otherwise.
constexpr std::string_view ENGINE_URL = "https://github.com/msagca/kuki.git";
/// Where a game keeps the engine, which every command after `new` looks for.
constexpr std::string_view ENGINE_PATH = "external/kuki";
constexpr std::string_view USAGE = R"(Usage: kuki <command> [options]

  new <game_name> [--engine <url|path>]
      Make a repository for a game in ./<game_name>: git, the engine as the submodule
      external/kuki at its latest main, and the engine's template, named for the game.
      The name is letters, digits, '-' and '_': `my-game` is the namespace my_game and
      the class MyGame. Nothing is committed; the first commit is yours.
  update [<ref>]
      Move external/kuki to the engine's latest main, or to <ref> (a commit, a tag, or
      origin/<branch>), with its own submodules, and stage the change.
  build [--release]
      Configure and build the game: `cmake --workflow Debug`, or Release.
  run [--release] [<argument>...]
      Run the built game, handing it the arguments (`--help` lists the engine's).
  --version
      Print the version of the engine this kuki came with.
)";
/// `git` with what every command here needs of it. On Windows that is `core.longpaths`: the engine's
/// submodules hold files deep enough -- assimp's bundled draco -- that a repository a few folders
/// down from the drive passes the 260 characters Windows allows a path unless git is told to ask
/// for more, and the clone fails with "Filename too long". Given with `-c`, which reaches the git
/// processes this one starts as well, so the submodules' clones get it too.
auto Git(std::initializer_list<std::string> arguments) -> std::vector<std::string> {
  std::vector<std::string> command{"git"};
#ifdef _WIN32
  command.insert(command.end(), {"-c", "core.longpaths=true"});
#endif
  command.insert(command.end(), arguments);
  return command;
}
auto Fail(const std::string &message) -> int {
  std::fprintf(stderr, "kuki: %s\n", message.c_str());
  return 1;
}
/// The game's repository: the nearest directory, up from here, with the engine in it.
auto FindGame() -> std::optional<fs::path> {
  std::error_code error;
  for (auto directory = fs::current_path(error); !error && !directory.empty(); directory = directory.parent_path()) {
    if (fs::exists(directory / ENGINE_PATH / "CMakeLists.txt") && fs::exists(directory / "CMakeLists.txt"))
      return directory;
    if (directory == directory.root_path())
      break;
  }
  return std::nullopt;
}
auto ReadFile(const fs::path &path) -> std::string {
  std::ifstream file(path, std::ios::binary);
  std::ostringstream text;
  text << file.rdbuf();
  return text.str();
}
auto WriteFile(const fs::path &path, const std::string &text) -> bool {
  std::ofstream file(path, std::ios::binary);
  file << text;
  return static_cast<bool>(file);
}
/// Copies the template into the game's repository, every file name and every file's text renamed.
auto CopyTemplate(const fs::path &from, const fs::path &to, const GameName &name) -> bool {
  std::error_code error;
  for (const auto &entry : fs::recursive_directory_iterator(from, error)) {
    const auto relative = fs::relative(entry.path(), from);
    const auto target = to / fs::path(RenameTemplate(relative.generic_string(), name));
    if (entry.is_directory()) {
      fs::create_directories(target, error);
      continue;
    }
    fs::create_directories(target.parent_path(), error);
    if (!WriteFile(target, RenameTemplate(ReadFile(entry.path()), name))) {
      std::fprintf(stderr, "kuki: could not write %s\n", target.string().c_str());
      return false;
    }
  }
  return !error;
}
auto New(const std::span<const std::string> arguments) -> int {
  std::optional<std::string> given;
  std::string engine{ENGINE_URL};
  for (size_t i = 0; i < arguments.size(); ++i) {
    if (arguments[i] == "--engine" && i + 1 < arguments.size())
      engine = arguments[++i];
    else if (arguments[i].starts_with("-"))
      return Fail("new: unknown option " + arguments[i]);
    else if (!given)
      given = arguments[i];
    else
      return Fail("new: one name only");
  }
  if (!given)
    return Fail("new: which game? `kuki new <game_name>`");
  const auto name = ParseGameName(*given);
  if (!name)
    return Fail(name.error());
  const auto root = fs::absolute(name->directory);
  std::error_code error;
  if (fs::exists(root) && !fs::is_empty(root, error))
    return Fail(root.string() + " already has something in it");
  if (!IsAvailable({"git", "--version"}))
    return Fail("git is needed and was not found on the PATH");
  // Without it the engine's binary assets -- the font -- arrive as pointers to them, and the game
  // starts with nothing written on screen. Said now rather than discovered then.
  if (!IsAvailable({"git", "lfs", "version"}))
    std::fprintf(stderr, "kuki: Git LFS was not found, so the engine's binary assets will be missing. Install it, then run `git -C %s lfs pull`.\n", (root / ENGINE_PATH).string().c_str());
  std::printf("Making %s in %s\n", name->pascal.c_str(), root.string().c_str());
  // Before git writes to the console, or the line comes after everything git said.
  std::fflush(stdout);
  if (Run(Git({"init", "--quiet", root.string()})) != 0)
    return Fail("git init failed");
  // Everything from here leaves a repository half made if it fails, which is said with the failure.
  const auto Abandon = [&](const std::string &message) {
    return Fail(message + ". Remove " + root.string() + " before trying again.");
  };
  // A local engine -- a checkout being worked on, or the tests' -- is a path, which git refuses to
  // clone a submodule from unless told it may, since a repository's own .gitmodules could otherwise
  // point anywhere on the machine. Only for that path, and only for this command.
  auto add = Git({});
  if (fs::is_directory(engine, error))
    add.insert(add.end(), {"-c", "protocol.file.allow=always"});
  add.insert(add.end(), {"submodule", "add", engine, std::string(ENGINE_PATH)});
  if (Run(add, root) != 0)
    return Abandon("could not add the engine from " + engine);
  if (Run(Git({"submodule", "update", "--init", "--recursive"}), root / ENGINE_PATH) != 0)
    return Abandon("could not fetch the engine's own submodules");
#ifdef _WIN32
  // Kept in the repositories' own configuration too, so a checkout or a status run by hand later
  // does not trip over the same paths.
  Run(Git({"config", "core.longpaths", "true"}), root);
  Run(Git({"submodule", "foreach", "--quiet", "--recursive", "git config core.longpaths true"}), root);
#endif
  const auto from = root / ENGINE_PATH / "template";
  if (!fs::is_directory(from))
    return Abandon("the engine at " + engine + " has no template/ -- it is older than `kuki new`");
  if (!CopyTemplate(from, root, *name))
    return Abandon("could not copy the template");
  // The engine's formatting, so the game's code reads as the engine's does.
  fs::copy_file(root / ENGINE_PATH / ".clang-format", root / ".clang-format", fs::copy_options::overwrite_existing, error);
  if (Run(Git({"add", "--all"}), root) != 0)
    return Abandon("could not stage the new files");
  std::printf("\n%s is ready, staged and not committed. Next:\n\n  cd %s\n  kuki build\n  kuki run\n", name->pascal.c_str(), name->directory.c_str());
  return 0;
}
auto Update(const std::span<const std::string> arguments) -> int {
  if (arguments.size() > 1)
    return Fail("update: one ref at most");
  const auto root = FindGame();
  if (!root)
    return Fail("not in a game's repository: no external/kuki here or above");
  const auto engine = *root / ENGINE_PATH;
  // `origin/HEAD` is the remote's default branch as it was cloned: main.
  const auto ref = arguments.empty() ? std::string("origin/HEAD") : arguments.front();
  std::printf("Was: ");
  std::fflush(stdout);
  Run(Git({"log", "--oneline", "-1"}), engine);
  if (Run(Git({"fetch", "--quiet", "origin"}), engine) != 0)
    return Fail("could not fetch the engine");
  if (Run(Git({"checkout", "--quiet", "--detach", ref}), engine) != 0)
    return Fail("could not check out " + ref + " in the engine");
  if (Run(Git({"submodule", "update", "--init", "--recursive"}), engine) != 0)
    return Fail("could not update the engine's own submodules");
  std::printf("Now: ");
  std::fflush(stdout);
  Run(Git({"log", "--oneline", "-1"}), engine);
  if (Run(Git({"add", std::string(ENGINE_PATH)}), *root) != 0)
    return Fail("could not stage the engine's new commit");
  std::printf("Staged. Build and run it, then commit.\n");
  return 0;
}
auto Configuration(std::span<const std::string> &arguments) -> std::string {
  if (!arguments.empty() && arguments.front() == "--release") {
    arguments = arguments.subspan(1);
    return "Release";
  }
  return "Debug";
}
auto Build(std::span<const std::string> arguments) -> int {
  const auto configuration = Configuration(arguments);
  if (!arguments.empty())
    return Fail("build: unknown option " + arguments.front());
  const auto root = FindGame();
  if (!root)
    return Fail("not in a game's repository: no external/kuki here or above");
  return Run({"cmake", "--workflow", configuration}, *root);
}
/// The game's executable, wherever the generator put it: `build/game/<Configuration>/` for Visual
/// Studio, which builds every configuration into one tree, or `build/game/` for one that does not.
auto FindExecutable(const fs::path &root, const std::string &configuration) -> std::optional<fs::path> {
  for (const auto &directory : {root / "build" / "game" / configuration, root / "build" / "game"}) {
    std::error_code error;
    for (const auto &entry : fs::directory_iterator(directory, error)) {
      if (!entry.is_regular_file())
        continue;
#ifdef _WIN32
      if (entry.path().extension() == ".exe")
        return entry.path();
#else
      if ((entry.status().permissions() & fs::perms::owner_exec) != fs::perms::none)
        return entry.path();
#endif
    }
  }
  return std::nullopt;
}
auto RunGame(std::span<const std::string> arguments) -> int {
  const auto configuration = Configuration(arguments);
  const auto root = FindGame();
  if (!root)
    return Fail("not in a game's repository: no external/kuki here or above");
  const auto executable = FindExecutable(*root, configuration);
  if (!executable)
    return Fail("no " + configuration + " build of the game yet: `kuki build" + (configuration == "Release" ? " --release" : "") + "` first");
  std::vector<std::string> command{executable->string()};
  command.insert(command.end(), arguments.begin(), arguments.end());
  return Run(command, *root);
}
} // namespace
auto main(const int argc, char **argv) -> int {
  const std::vector<std::string> arguments(argv + 1, argv + argc);
  if (arguments.empty() || arguments.front() == "help" || arguments.front() == "--help" || arguments.front() == "-h") {
    std::fputs(USAGE.data(), arguments.empty() ? stderr : stdout);
    return arguments.empty() ? 1 : 0;
  }
  const auto &command = arguments.front();
  const auto rest = std::span(arguments).subspan(1);
  if (command == "--version") {
    std::printf("kuki %s\n", KUKI_VERSION);
    return 0;
  }
  if (command == "new")
    return New(rest);
  if (command == "update")
    return Update(rest);
  if (command == "build")
    return Build(rest);
  if (command == "run")
    return RunGame(rest);
  return Fail("unknown command '" + command + "'. `kuki --help` lists them.");
}
