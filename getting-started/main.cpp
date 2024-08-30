#include "shader.hpp"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/detail/func_trigonometric.inl>
#include <glm/ext/matrix_clip_space.inl>
#include <glm/ext/matrix_transform.inl>
#include <iostream>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
auto deltaTime = 0.0f;
// window parameters
const unsigned int WINDOW_HEIGHT = 600;
const unsigned int WINDOW_WIDTH = 800;
// camera parameters
const auto CAMERA_SPEED = 2.5f;
const auto FAR_PLANE = 100.0f;
const auto NEAR_PLANE = .1f;
const auto VIEW_ANGLE = 45.0f;
// camera variables
auto cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
auto cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
auto cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
// mouse parameters
const auto MOUSE_SENSITIVITY = 0.1f;
const auto MOUSE_PITCH_LIMIT = 89.0f;
// mouse variables
auto mouseFirstEnter = true;
auto mousePitch = 0.0f;
auto mouseXLast = (float)WINDOW_WIDTH / 2;
auto mouseYLast = (float)WINDOW_HEIGHT / 2;
auto mouseYaw = -90.0f;
static void ProcessInput(GLFWwindow* window) {
  auto cameraSpeed = CAMERA_SPEED * deltaTime;
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    glfwSetWindowShouldClose(window, true);
  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    cameraPos += cameraSpeed * cameraFront;
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    cameraPos -= cameraSpeed * cameraFront;
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
}
static void MouseCallback(GLFWwindow* window, double xpos, double ypos) {
  if (mouseFirstEnter) {
    mouseXLast = xpos;
    mouseYLast = ypos;
    mouseFirstEnter = false;
  }
  auto xoffset = xpos - mouseXLast;
  auto yoffset = mouseYLast - ypos;
  mouseXLast = xpos;
  mouseYLast = ypos;
  xoffset *= MOUSE_SENSITIVITY;
  yoffset *= MOUSE_SENSITIVITY;
  mouseYaw += xoffset;
  mousePitch += yoffset;
  if (mousePitch > MOUSE_PITCH_LIMIT)
    mousePitch = MOUSE_PITCH_LIMIT;
  if (mousePitch < -MOUSE_PITCH_LIMIT)
    mousePitch = -MOUSE_PITCH_LIMIT;
  glm::vec3 direction;
  direction.x = cos(glm::radians(mouseYaw)) * cos(glm::radians(mousePitch));
  direction.y = sin(glm::radians(mousePitch));
  direction.z = sin(glm::radians(mouseYaw)) * cos(glm::radians(mousePitch));
  cameraFront = glm::normalize(direction);
}
static void FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
  glViewport(0, 0, width, height);
}
static void CreateTexture(const char* filename) {
  unsigned int texture;
  glGenTextures(1, &texture);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  int width, height, nrChannels;
  auto data = stbi_load(filename, &width, &height, &nrChannels, 0);
  if (data) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
  } else
    std::cout << "Error: Failed to load texture." << std::endl;
  stbi_image_free(data);
}
int main() {
  // create a window
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  auto window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Getting Started", NULL, NULL);
  if (window == NULL)
    std::cout << "Error: Failed to create GLFW window." << std::endl;
  glfwMakeContextCurrent(window);
  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    std::cout << "Error: Failed to initialize GLAD." << std::endl;
  glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
  glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
  Shader shader("shader.vert", "shader.frag");
  CreateTexture("container.jpg");
  // define a mesh
  //
  //              0-----1
  //              |     |
  //              |     |
  //  1-----0-----4-----5-----1
  //  |     |     |     |     |
  //  |     |     |     |     |
  //  2-----3-----7-----6-----2
  //              |     |
  //              |     |
  //              3-----2
  //
  // per vertex data: x, y, z, u, v
  float vertices[] = {-0.5f, -0.5f, -0.5f, 0.0f, 0.0f, 0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.5f, 0.5f, -0.5f, 1.0f, 1.0f, 0.5f, 0.5f, -0.5f, 1.0f, 1.0f, -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.5f, 0.5f, 0.5f, 1.0f, 1.0f, 0.5f, 0.5f, 0.5f, 1.0f, 1.0f, -0.5f, 0.5f, 0.5f, 0.0f, 1.0f, -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, -0.5f, 0.5f, 0.5f, 1.0f, 0.0f, -0.5f, 0.5f, -0.5f, 1.0f, 1.0f, -0.5f, -0.5f, -0.5f, 0.0f, 1.0f, -0.5f, -0.5f, -0.5f, 0.0f, 1.0f, -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, -0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.5f, 0.5f, -0.5f, 1.0f, 1.0f, 0.5f, -0.5f, -0.5f, 0.0f, 1.0f, 0.5f, -0.5f, -0.5f, 0.0f, 1.0f, 0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 0.5f, 0.5f, 0.5f, 1.0f, 0.0f, -0.5f, -0.5f, -0.5f, 0.0f, 1.0f, 0.5f, -0.5f, -0.5f, 1.0f, 1.0f, 0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.5f, -0.5f, 0.5f, 1.0f, 0.0f, -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, -0.5f, -0.5f, -0.5f, 0.0f, 1.0f, -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.5f, 0.5f, -0.5f, 1.0f, 1.0f, 0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.5f, 0.5f, 0.5f, 1.0f, 0.0f, -0.5f, 0.5f, 0.5f, 0.0f, 0.0f, -0.5f, 0.5f, -0.5f, 0.0f, 1.0f};
  unsigned int VAO, VBO;
  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);
  glBindVertexArray(VAO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
  shader.Use();
  // pass the vertex positions
  auto posAttrib = glGetAttribLocation(shader.ID, "posIn");
  glVertexAttribPointer(posAttrib, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(posAttrib);
  // pass the UV coordinates for texture mapping
  auto texCoordAttrib = glGetAttribLocation(shader.ID, "texCoordIn");
  glVertexAttribPointer(texCoordAttrib, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
  glEnableVertexAttribArray(texCoordAttrib);
  glEnable(GL_DEPTH_TEST);
  // set up the mouse
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  glfwSetCursorPosCallback(window, MouseCallback);
  // initialize the transform matrices
  auto model = glm::mat4(1.0f);
  auto view = glm::mat4(1.0f);
  view = glm::translate(view, -cameraPos);
  auto projection = glm::perspective(glm::radians(VIEW_ANGLE), (float)WINDOW_WIDTH / WINDOW_HEIGHT, NEAR_PLANE, FAR_PLANE);
  // send the new values to the shader
  shader.SetMat4("model", model);
  shader.SetMat4("view", view);
  shader.SetMat4("projection", projection);
  glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
  auto timeLast = 0.0f;
  while (!glfwWindowShouldClose(window)) {
    auto timeNow = glfwGetTime();
    deltaTime = timeNow - timeLast;
    timeLast = timeNow;
    ProcessInput(window);
    // re-calculate the view matrix after cursor movement
    view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
    shader.SetMat4("view", view);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glfwSwapBuffers(window);
    glfwPollEvents();
  }
  // clean up
  glDeleteVertexArrays(1, &VAO);
  glDeleteBuffers(1, &VBO);
  glDeleteProgram(shader.ID);
  glfwTerminate();
  return 0;
}
