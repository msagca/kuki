#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <gl_context.hpp>
#include <glad/glad.h>
#include <spdlog/spdlog.h>
#include <string>
namespace kuki {
static constexpr auto GL_MAJOR = 4;
static constexpr auto GL_MINOR = 6;
auto GLContext::ApplyWindowHints() const -> void {
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, GL_MAJOR);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, GL_MINOR);
  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  // No multisampling on the default framebuffer, deliberately. The graph does its own: the scene
  // is drawn into a 4x `SceneMulti` target and resolved, so samples here would antialias a
  // full-screen blit of an already-resolved image and nothing else.
  //
  // They also made that blit illegal. `glBlitFramebuffer` into a multisampled draw framebuffer
  // must be one-to-one, so presenting a render target of any other size failed with
  // GL_INVALID_OPERATION -- once per frame, saying only that "dimensions must be identical with
  // the current filtering modes".
}
auto GLContext::Initialize(GLFWwindow *window) -> bool {
  this->window = window;
  glfwMakeContextCurrent(window);
  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    spdlog::error("[GLContext] failed to initialize GLAD.");
    return false;
  }
  auto version = reinterpret_cast<const char *>(glGetString(GL_VERSION));
  spdlog::info("[OpenGL] version: {}", version);
  int major, minor;
  glGetIntegerv(GL_MAJOR_VERSION, &major);
  glGetIntegerv(GL_MINOR_VERSION, &minor);
  if (major < GL_MAJOR || (major == GL_MAJOR && minor < GL_MINOR)) {
    spdlog::error("[OpenGL] version {}.{} or higher is required.", GL_MAJOR, GL_MINOR);
    return false;
  }
  glfwSwapInterval(0);
  int width, height;
  glfwGetFramebufferSize(window, &width, &height);
  Resize(width, height);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glCullFace(GL_BACK);
  glEnable(GL_CULL_FACE);
  glEnable(GL_DEBUG_OUTPUT);
  glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_MULTISAMPLE);
  glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
  glFrontFace(GL_CCW);
  glDebugMessageCallback(DebugMessageCallback, nullptr);
  return true;
}
auto GLContext::Present() -> void {
  if (window)
    glfwSwapBuffers(window);
}
auto GLContext::OnResize(const int width, const int height) -> void {
  glViewport(0, 0, width, height);
}
auto GLContext::SetVSync(const bool enabled) -> void {
  glfwSwapInterval(enabled ? 1 : 0);
}
auto GLContext::Shutdown() -> void {
  window = nullptr;
}
auto GLContext::DebugMessageCallback(unsigned int source, unsigned int type, unsigned int id, unsigned int severity, int length, const char *message, const void *userParam) -> void {
  std::string sourceStr, typeStr;
  switch (source) {
  case GL_DEBUG_SOURCE_API:
    sourceStr = "API";
    break;
  case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
    sourceStr = "window system";
    break;
  case GL_DEBUG_SOURCE_SHADER_COMPILER:
    sourceStr = "shader compiler";
    break;
  case GL_DEBUG_SOURCE_THIRD_PARTY:
    sourceStr = "third party";
    break;
  case GL_DEBUG_SOURCE_APPLICATION:
    sourceStr = "application";
    break;
  default:
    sourceStr = "other";
    break;
  }
  switch (type) {
  case GL_DEBUG_TYPE_ERROR:
    typeStr = "error";
    break;
  case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
    typeStr = "deprecated behavior";
    break;
  case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
    typeStr = "undefined behavior";
    break;
  case GL_DEBUG_TYPE_PORTABILITY:
    typeStr = "portability";
    break;
  case GL_DEBUG_TYPE_PERFORMANCE:
    typeStr = "performance";
    break;
  case GL_DEBUG_TYPE_MARKER:
    typeStr = "marker";
    break;
  case GL_DEBUG_TYPE_PUSH_GROUP:
    typeStr = "push group";
    break;
  case GL_DEBUG_TYPE_POP_GROUP:
    typeStr = "pop group";
    break;
  default:
    typeStr = "other";
    break;
  }
  switch (severity) {
  case GL_DEBUG_SEVERITY_HIGH:
    spdlog::error("[OpenGL] {} {}: {}", sourceStr, typeStr, message);
    break;
  case GL_DEBUG_SEVERITY_MEDIUM:
    spdlog::warn("[OpenGL] {} {}: {}", sourceStr, typeStr, message);
    break;
  case GL_DEBUG_SEVERITY_LOW:
    spdlog::info("[OpenGL] {} {}: {}", sourceStr, typeStr, message);
    break;
  default:
    spdlog::debug("[OpenGL] {} {}: {}", sourceStr, typeStr, message);
  }
}
} // namespace kuki
