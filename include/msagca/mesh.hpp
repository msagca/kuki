#pragma once
#include <vector>
#include <component.hpp>
MeshFilter CreateMesh(const std::vector<float>&);
MeshFilter CreateMesh(const std::vector<float>& vertices, const std::vector<unsigned int>& indices);
