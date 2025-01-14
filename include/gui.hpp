#pragma once
#include <camera_controller.hpp>
#include <entity_manager.hpp>
#include <input_manager.hpp>
void DisplayFPS(unsigned int);
void DisplayKeyBindings(InputManager&);
void DisplayHierarchy(EntityManager&, InputManager&, CameraController&);
void DisplayCreateMenu(EntityManager&, bool&);
