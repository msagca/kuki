#pragma once
#include <application.hpp>
#include <component_type.hpp>
#include <id.hpp>
namespace kuki {
class Application;
}
auto AddComponentByType(kuki::Application &, const kuki::EntityID, const kuki::ComponentType) -> void;
auto RemoveComponentByType(kuki::Application &, const kuki::EntityID, const kuki::ComponentType) -> bool;
