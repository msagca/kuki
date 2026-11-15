#include <algorithm>
#include <array>
#include <cctype>
#include <expected>
#include <format>
#include <game_name.hpp>
#include <string>
#include <string_view>
#include <vector>
namespace kuki::cli {
namespace {
auto IsUpper(const char c) -> bool {
  return std::isupper(static_cast<unsigned char>(c)) != 0;
}
auto IsLower(const char c) -> bool {
  return std::islower(static_cast<unsigned char>(c)) != 0;
}
auto IsDigit(const char c) -> bool {
  return std::isdigit(static_cast<unsigned char>(c)) != 0;
}
auto Lower(std::string word) -> std::string {
  std::ranges::transform(word, word.begin(), [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return word;
}
auto Capitalized(std::string word) -> std::string {
  if (!word.empty())
    word[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(word[0])));
  return word;
}
/// Words at the separators, and inside a word at a lower-case letter or digit followed by a capital
/// (`myGame`), or at the last capital of a run followed by a lower-case letter (`HTTPServer`).
auto Words(const std::string_view name) -> std::vector<std::string> {
  std::vector<std::string> words;
  std::string word;
  for (size_t i = 0; i < name.size(); ++i) {
    const auto c = name[i];
    if (c == '-' || c == '_') {
      if (!word.empty())
        words.push_back(std::move(word));
      word.clear();
      continue;
    }
    if (!word.empty() && IsUpper(c)) {
      const auto previous = word.back();
      const auto next = i + 1 < name.size() ? name[i + 1] : '\0';
      if (IsLower(previous) || IsDigit(previous) || (IsUpper(previous) && IsLower(next))) {
        words.push_back(std::move(word));
        word.clear();
      }
    }
    word.push_back(c);
  }
  if (!word.empty())
    words.push_back(std::move(word));
  return words;
}
/// Namespaces a game's cannot be: C++'s keywords, which do not compile as one, and the engine's and
/// the standard library's, which would put the game's names among theirs.
constexpr std::array RESERVED{"alignas", "alignof", "and", "asm", "auto", "bool", "break", "case", "catch", "char", "class", "concept", "const", "consteval", "constexpr", "constinit", "continue", "decltype", "default", "delete", "do", "double", "else", "enum", "explicit", "export", "extern", "false", "float", "for", "friend", "goto", "if", "inline", "int", "long", "module", "mutable", "namespace", "new", "noexcept", "not", "nullptr", "operator", "or", "private", "protected", "public", "register", "requires", "return", "short", "signed", "sizeof", "static", "struct", "switch", "template", "this", "throw", "true", "try", "typedef", "typeid", "typename", "union", "unsigned", "using", "virtual", "void", "volatile", "while", "kuki", "std", "glm", "spdlog"};
auto ReplaceAll(std::string text, const std::string_view from, const std::string_view to) -> std::string {
  for (auto at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size()))
    text.replace(at, from.size(), to);
  return text;
}
} // namespace
auto ParseGameName(const std::string_view name) -> std::expected<GameName, std::string> {
  if (name.empty())
    return std::unexpected("A game needs a name");
  if (!std::isalpha(static_cast<unsigned char>(name[0])))
    return std::unexpected(std::format("'{}' does not start with a letter, so it cannot name a class", name));
  if (const auto bad = std::ranges::find_if(name, [](const char c) { return !std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_'; }); bad != name.end())
    return std::unexpected(std::format("'{}' has a '{}' in it: a name is letters, digits, '-' and '_'", name, *bad));
  const auto words = Words(name);
  GameName result{.directory = std::string(name)};
  for (const auto &word : words) {
    result.snake += (result.snake.empty() ? "" : "_") + Lower(word);
    result.pascal += Capitalized(word);
    result.title += (result.title.empty() ? "" : " ") + Capitalized(word);
  }
  if (std::ranges::find(RESERVED, result.snake) != RESERVED.end())
    return std::unexpected(std::format("'{}' cannot be a game's namespace: C++ or the engine already uses it", result.snake));
  return result;
}
auto RenameTemplate(const std::string_view text, const GameName &name) -> std::string {
  // None of the three is inside another, so the order does not matter.
  auto result = ReplaceAll(std::string(text), "My Game", name.title);
  result = ReplaceAll(std::move(result), "MyGame", name.pascal);
  return ReplaceAll(std::move(result), "my_game", name.snake);
}
} // namespace kuki::cli
