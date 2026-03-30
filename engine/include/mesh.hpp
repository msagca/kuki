#pragma once
#include <primitive.hpp>
namespace kuki {
struct Mesh {
  std::vector<Vertex> vertices;
  std::vector<unsigned int> indices;
};
} // namespace kuki
