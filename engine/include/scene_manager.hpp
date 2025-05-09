#pragma once
#include <kuki_engine_export.h>
#include <scene.hpp>
#include <string>
#include <unordered_map>
namespace kuki {
class KUKI_ENGINE_API SceneManager {
private:
  unsigned int nextId{0};
  std::unordered_map<unsigned int, Scene*> idToScene;
  std::unordered_map<std::string, unsigned int> nameToId;
public:
  ~SceneManager();
  unsigned int Create(const std::string&);
  void Delete(unsigned int);
  void Delete(const std::string&);
  Scene* Get(unsigned int);
  Scene* Get(const std::string&);
  bool Has(unsigned int);
  bool Has(const std::string&);
};
} // namespace kuki
