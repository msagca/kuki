#include <component/component.hpp>
#include <string>
Property::Property(const std::string& name, PropertyValue value, PropertyType type)
  : name(name), value(value), type(type) {}
