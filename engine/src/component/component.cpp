#include <component/component.hpp>
Property::Property(const std::string& name, PropertyValue value, PropertyType type)
  : name(name), value(value), type(type) {}
