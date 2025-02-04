#pragma once
#include "component.hpp"
#include <glad/glad.h>
#include <string>
#include <variant>
#include <vector>
struct Mesh : IComponent {
  unsigned int vertexArray{};
  unsigned int vertexBuffer{};
  unsigned int indexBuffer{};
  int vertexCount{}; // NOTE: includes duplicates if no EBO is used
  int indexCount{};
  std::string GetName() const override {
    return "Mesh";
  }
  std::vector<Property> GetProperties() const override {
    return {{"VertexArray", vertexArray}, {"VertexBuffer", vertexBuffer}, {"IndexBuffer", indexBuffer}, {"VertexCount", vertexCount}, {"IndexCount", indexCount}};
  }
  void SetProperty(Property property) override {
    if (std::holds_alternative<unsigned int>(property.value)) {
      auto& value = std::get<unsigned int>(property.value);
      if (property.name == "VertexArray")
        vertexArray = value;
      else if (property.name == "VertexBuffer")
        vertexBuffer = value;
      else if (property.name == "IndexBuffer")
        indexBuffer = value;
    } else if (std::holds_alternative<int>(property.value)) {
      auto& value = std::get<int>(property.value);
      if (property.name == "VertexCount")
        vertexCount = value;
      else if (property.name == "IndexCount")
        indexCount = value;
    }
  }
};
