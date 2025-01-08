#pragma once
#include <entity_manager.hpp>
#include <functional>
#include <input_manager.hpp>
#include <string>
#include <vector>
extern bool showHierarchyWindow;
extern bool showFPS;
struct Hint {
  std::string text;
  std::function<bool()> condition;
};
static const std::vector<Hint> hints = {
  {"Hold down the right mouse button and use the WASD keys to fly around.", []() { return true; }},
  {"Press [H] to show/hide the hierarchy window.", []() { return !showHierarchyWindow; }},
  {"Press [R] to change the render mode.", []() { return true; }},
  {"Press [F] to display the FPS counter.", []() { return !showFPS; }}};
void ShowHints();
void ShowFPS(unsigned int);
void ShowHierarchyWindow(EntityManager&, InputManager&);
