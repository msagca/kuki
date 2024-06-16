#include <GLFW/glfw3.h>
#include <GL/gl.h>
int main(void) {
  GLFWwindow *window;
  if (!glfwInit())
    return -1;
  window = glfwCreateWindow(800, 480, "Project 1", NULL, NULL);
  if (!window) {
    glfwTerminate();
    return -1;
  }
  glfwMakeContextCurrent(window);
  struct Color {
    float red = .2f;
    float green = .2f;
    float blue = .2f;
    float alpha = 1;
  } color;
  glClearColor(color.red, color.green, color.blue, color.alpha);
  while (!glfwWindowShouldClose(window)) {
    glClear(GL_COLOR_BUFFER_BIT);
    glfwSwapBuffers(window);
    glfwPollEvents();
  }
  glfwTerminate();
  return 0;
}
