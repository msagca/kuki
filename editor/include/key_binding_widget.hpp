#pragma once
#include <string>
namespace kuki {
class Application;
}
auto DisplayKeyBindingRow(kuki::Application &, const char *label, const std::string &description, const std::string &bindingName, int &rebindingIndex, int index) -> void;
auto DisplaySequenceRow(const std::string &sequence, const std::string &description) -> void;
