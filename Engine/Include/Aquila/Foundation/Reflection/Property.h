#ifndef AQUILA_FOUNDATION_REFLECTION_PROPERTY_H
#define AQUILA_FOUNDATION_REFLECTION_PROPERTY_H

#include "Aquila/Foundation/PrimitiveTypes.h"

#include <glm/glm.hpp>

#include "Aquila/Foundation/Math/MathTypes.h"

#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace Aquila::Reflection {

using PropertyValue = std::variant<bool, Int32, F32, Vec2, Vec3, Vec4, std::string>;

enum class PropertyKind : Uint8 { Bool, Int, Float, Vec2, Vec3, Vec4, String, Enum };

struct EnumOption {
	std::string name;
	Int32 value = 0;
};

struct PropertyHints {
	F32 min = std::numeric_limits<F32>::lowest();
	F32 max = std::numeric_limits<F32>::max();
	F32 speed = 0.1F;
	Int32 precision = 2;
	bool color = false;
	bool toggle = false;
	bool slider = false;
	bool read_only = false;
	std::string_view unit{};
	std::string_view asset_type{};
};

struct Property {
	std::string name;
	PropertyKind kind = PropertyKind::Float;
	PropertyHints hints;
	std::vector<EnumOption> options;

	Delegate<PropertyValue(const void *)> get;
	Delegate<void(void *, const PropertyValue &)> set;
	Delegate<bool(const void *)> visible;

	[[nodiscard]] bool is_visible(const void *instance) const { return !visible || visible(instance); }
};

namespace detail {

template <typename V> constexpr PropertyKind kind_of() {
	if constexpr (std::is_same_v<V, bool>) {
		return PropertyKind::Bool;
	} else if constexpr (std::is_enum_v<V>) {
		return PropertyKind::Enum;
	} else if constexpr (std::is_integral_v<V>) {
		return PropertyKind::Int;
	} else if constexpr (std::is_floating_point_v<V>) {
		return PropertyKind::Float;
	} else if constexpr (std::is_same_v<V, Vec2>) {
		return PropertyKind::Vec2;
	} else if constexpr (std::is_same_v<V, Vec3>) {
		return PropertyKind::Vec3;
	} else if constexpr (std::is_same_v<V, Vec4>) {
		return PropertyKind::Vec4;
	} else if constexpr (std::is_same_v<V, std::string>) {
		return PropertyKind::String;
	} else {
		static_assert(!sizeof(V), "Type cannot be exposed as a reflected property");
	}
}

template <typename V> PropertyValue to_value(const V &value) {
	if constexpr (std::is_same_v<V, bool>) {
		return value;
	} else if constexpr (std::is_enum_v<V> || std::is_integral_v<V>) {
		return static_cast<Int32>(value);
	} else if constexpr (std::is_floating_point_v<V>) {
		return static_cast<F32>(value);
	} else {
		return value;
	}
}

template <typename V> V from_value(const PropertyValue &value) {
	if constexpr (std::is_same_v<V, bool>) {
		return std::get<bool>(value);
	} else if constexpr (std::is_enum_v<V> || std::is_integral_v<V>) {
		return static_cast<V>(std::get<Int32>(value));
	} else if constexpr (std::is_floating_point_v<V>) {
		return static_cast<V>(std::get<F32>(value));
	} else {
		return std::get<V>(value);
	}
}

} // namespace detail

} // namespace Aquila::Reflection

#endif
