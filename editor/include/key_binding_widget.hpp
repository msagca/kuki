#pragma once
#include <string>
class Editor;
auto DisplayKeyBindingRow(Editor &, const char *label, const std::string &description, const std::string &bindingName, int &rebindingIndex, int index) -> void;
auto DisplaySequenceRow(const std::string &sequence, const std::string &description) -> void;
