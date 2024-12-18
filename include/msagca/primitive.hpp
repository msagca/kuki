#pragma once
#include <vector>
#include <glm/vec3.hpp>
struct Triangle {
  glm::vec3 v1;
  glm::vec3 v2;
  glm::vec3 v3;
};
class Primitive {
private:
  static std::vector<Triangle> CreateIcosahedron();
  static std::vector<Triangle> Subdivide(const std::vector<Triangle>&, int);
  static void FlipWindingOrder(std::vector<float>&, bool = false);
public:
  static std::vector<float> Cube();
  static std::vector<float> Sphere(int = 3);
};
