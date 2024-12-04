#pragma once
#include <entity.hpp>
#include <system.hpp>
#include <GLFW/glfw3.h>
extern double deltaTime;
extern Camera* cameraPtr;
extern RenderSystem* renderSystemPtr;
const auto WINDOW_HEIGHT = 600u;
const auto WINDOW_WIDTH = 800u;
unsigned int LoadTexture(const char*);
void ShowCreateMenu(EntityManager&);
void FramebufferSizeCallback(GLFWwindow*, int, int);
void MouseButtonCallback(GLFWwindow*, int, int, int);
void MousePosCallback(GLFWwindow*, double, double);
void ProcessInput(GLFWwindow*);
void SetWindowIcon(GLFWwindow*, const char*);
void WindowCloseCallback(GLFWwindow*);
