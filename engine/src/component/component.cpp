#include <component/component.hpp>
#include <string>
namespace kuki {
Property::Property(const std::string& name, PropertyValue value, PropertyType type)
  : name(name), value(value), type(type) {}
} // namespace kuki
