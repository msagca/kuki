#pragma once
#include <vector>
class Cube {
public:
  static std::vector<float> GetVertices(float size = 1.0f);
  static std::vector<unsigned int> GetIndices();
  static std::vector<float> GetNormals();
};
