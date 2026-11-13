#pragma once
#include <array>
#include <component_traits.hpp>
#include <cstddef>
#include <cstdint>
#include <glm/ext/quaternion_float.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <id.hpp>
#include <string_view>
#include <type_traits>
namespace kuki {
/// @brief What a saved field holds, which is all the scene file needs to know to write it.
enum class FieldKind : uint8_t {
  Bool,
  /// A signed integer or an enum over one, `size` bytes wide.
  Signed,
  /// An unsigned integer or an enum over one, `size` bytes wide.
  Unsigned,
  /// A `float` or a `double`, `size` bytes wide.
  Float,
  Vec2,
  Vec3,
  Vec4,
  Quat,
  /// Saved as the place in the file of the entity it names, as an IK goal's target is: an id is
  /// only the number this run gave the entity, and loading numbers every entity afresh.
  Entity,
  /// Saved as the id, and listed in the scene's manifest so the asset comes with it.
  Asset,
};
/// @brief One member of a component, by the key it is saved under.
///
/// Type-erased, because the scene serializer is compiled into the engine and a game's components
/// are not: it reaches the member through `address`, which the game's own binary compiled, and
/// writes it by its `kind`. Made with `Field`, never by hand.
struct ComponentField {
  std::string_view key;
  FieldKind kind{};
  /// The width of a `Signed`, `Unsigned` or `Float` field, in bytes.
  uint8_t size{};
  /// The member, given the component it is a member of.
  using Address = auto (*)(void *component) -> void *;
  Address address{};
};
namespace detail {
  template <typename M>
  struct MemberPointer;
  template <typename C, typename M>
  struct MemberPointer<M C::*> {
    using Class = C;
    using Type = M;
  };
  template <typename>
  inline constexpr bool AlwaysFalse = false;
  template <typename T>
  consteval auto FieldKindOf() -> FieldKind {
    if constexpr (std::is_enum_v<T>)
      return FieldKindOf<std::underlying_type_t<T>>();
    else if constexpr (std::is_same_v<T, bool>)
      return FieldKind::Bool;
    else if constexpr (std::is_integral_v<T> && std::is_signed_v<T>)
      return FieldKind::Signed;
    else if constexpr (std::is_integral_v<T>)
      return FieldKind::Unsigned;
    else if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>)
      return FieldKind::Float;
    else if constexpr (std::is_same_v<T, glm::vec2>)
      return FieldKind::Vec2;
    else if constexpr (std::is_same_v<T, glm::vec3>)
      return FieldKind::Vec3;
    else if constexpr (std::is_same_v<T, glm::vec4>)
      return FieldKind::Vec4;
    else if constexpr (std::is_same_v<T, glm::quat>)
      return FieldKind::Quat;
    else if constexpr (std::is_same_v<T, EntityID>)
      return FieldKind::Entity;
    else if constexpr (std::is_same_v<T, AssetID>)
      return FieldKind::Asset;
    else
      static_assert(AlwaysFalse<T>, "a saved field must be a bool, a number, an enum, a glm vec2/vec3/vec4 or quat, an EntityID or an AssetID -- save what a struct or array holds as fields of their own");
  }
  template <auto Member>
  auto AddressOf(void *component) -> void * {
    using C = typename MemberPointer<decltype(Member)>::Class;
    return &(static_cast<C *>(component)->*Member);
  }
} // namespace detail
/// @brief A `ComponentField` that knows whose member it is, so that a component's list can be
/// checked to hold its own members and no other's.
template <typename C>
struct ComponentFieldOf {
  ComponentField field;
};
/// @brief One member of a component to save, under a key of its own.
///
/// `kuki::Field<&frontier::Wing::flock>("flock")`. The key is what the file says, and it is the
/// file's and not the code's: a member renamed keeps its key and the scenes saved before still
/// load, and a member added later is read as its default from a file that has no key for it.
template <auto Member>
requires std::is_member_object_pointer_v<decltype(Member)>
consteval auto Field(const std::string_view key) -> ComponentFieldOf<typename detail::MemberPointer<decltype(Member)>::Class> {
  using M = typename detail::MemberPointer<decltype(Member)>::Type;
  constexpr auto kind = detail::FieldKindOf<M>();
  constexpr uint8_t size = kind == FieldKind::Signed || kind == FieldKind::Unsigned || kind == FieldKind::Float ? sizeof(M) : 0;
  return {ComponentField{key, kind, size, &detail::AddressOf<Member>}};
}
/// @brief The members of a game's component that a scene is saved with. None, unless the game says.
///
/// A game that wants its component in its scene files says which members, after `KUKI_COMPONENT`
/// and at global scope as that is:
///
/// ```cpp
/// template <>
/// struct kuki::ComponentFields<frontier::Wing> {
///   static constexpr std::array fields{kuki::Field<&frontier::Wing::flock>("flock"), kuki::Field<&frontier::Wing::bird>("bird")};
/// };
/// ```
///
/// By name and not as the component's bytes, though every component is trivially copyable and the
/// bytes would be one line to write: a file of bytes is unreadable as a diff, and it stops loading
/// the day a field is added. A member left out is not saved and comes back as its default. The
/// engine's own components are written by the serializer, which knows them, and need none of this.
template <typename T>
struct ComponentFields {
  static constexpr std::array<ComponentFieldOf<T>, 0> fields{};
};
namespace detail {
  template <typename T>
  consteval auto KeysAreDistinct() -> bool {
    const auto &fields = ComponentFields<T>::fields;
    for (size_t a = 0; a < fields.size(); ++a)
      for (size_t b = a + 1; b < fields.size(); ++b)
        if (fields[a].field.key == fields[b].field.key)
          return false;
    return true;
  }
  template <typename T>
  consteval auto EraseFields() {
    using Declared = std::remove_cvref_t<decltype(ComponentFields<T>::fields)>;
    static_assert(std::is_same_v<typename Declared::value_type, ComponentFieldOf<T>>, "a component's saved fields must be its own members -- each Field<&T::member> names a member of the component it is declared for");
    static_assert(KeysAreDistinct<T>(), "two of a component's saved fields have the same key -- give each its own");
    std::array<ComponentField, std::tuple_size_v<Declared>> erased{};
    for (size_t index = 0; index < erased.size(); ++index)
      erased[index] = ComponentFields<T>::fields[index].field;
    return erased;
  }
} // namespace detail
/// @brief A component's saved fields, checked and with their owner forgotten: what a table's column
/// hands the scene serializer.
template <IsComponent T>
inline constexpr auto SavedFieldsOf = detail::EraseFields<T>();
} // namespace kuki
