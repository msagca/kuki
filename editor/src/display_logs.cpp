#include <editor.hpp>
#include <imgui.h>
#include <imgui_sink.hpp>
#include <string>
void Editor::DisplayLogs() {
  if (!imguiSink)
    return;
  ImGui::Begin("Logs");
  imguiSink->ForEachLine([](const std::string& line) { ImGui::TextUnformatted(line.c_str()); });
  ImGui::End();
}
