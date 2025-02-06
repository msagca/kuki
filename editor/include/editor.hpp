#pragma once
#include <application.hpp>
#include <camera_controller.hpp>
#include <GLFW/glfw3.h>
class Editor : public Application {
private:
  InputManager inputManager;
  CameraController cameraController;
  GLFWwindow* window;
  int selectedEntity = -1;
  void InitOpenGL(int, int);
  void InitImGui();
  void SetWindowIcon(const char*);
  void LoadDefaultScene();
  void LoadDefaultAssets();
  void DisplayScene();
  void DisplayHierarchy();
  void DisplayEntityHierarchy(unsigned int, int&, int&);
  void DisplayProperties(unsigned int);
  void DisplayCreateMenu();
  void Start() override;
  bool Status() override;
  void Update() override;
  void UpdateView();
  void Shutdown() override;
  static void WindowCloseCallback(GLFWwindow*);
  static void FramebufferSizeCallback(GLFWwindow*, int, int);
  static void KeyCallback(GLFWwindow*, int, int, int, int);
  static void MouseButtonCallback(GLFWwindow*, int, int, int);
  static void CursorPosCallback(GLFWwindow*, double, double);
public:
  Editor();
};
