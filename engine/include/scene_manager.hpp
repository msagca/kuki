#pragma once
#include <kuki_engine_export.h>
#include <scene.hpp>
#include <string>
#include <unordered_map>
namespace kuki {
class KUKI_ENGINE_API SceneManager {
private:
  size_t nextId{0};
  std::unordered_map<size_t, Scene*> idToScene;
  std::unordered_map<std::string, size_t> nameToId;
public:
  ~SceneManager();
  size_t Create(const std::string&);
  void Delete(size_t);
  void Delete(const std::string&);
  Scene* Get(size_t);
  Scene* Get(const std::string&);
  bool Has(size_t);
  bool Has(const std::string&);
};
} // namespace kuki
