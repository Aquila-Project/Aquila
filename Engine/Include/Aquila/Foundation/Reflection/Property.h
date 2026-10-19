#ifndef AQUILA_FOUNDATION_REFLECTION_PROPERTY_H
#define AQUILA_FOUNDATION_REFLECTION_PROPERTY_H

#include "Aquila/Foundation/PrimitiveTypes.h"

#include "Aquila/Foundation/Math/MathTypes.h"

namespace Aquila::Reflection {

/// Every reflected value travels as one of these. All integers and enums become Int32, all floats F32.
using PropertyValue = std::variant<bool, Int32, F32, Vec2, Vec3, Vec4, std::string>;

/// Enum values travel as Int32, their names are in Property::options.
enum class PropertyKind : Uint8 { Bool, Int, Float, Vec2, Vec3, Vec4, String, Enum };

struct EnumOption {
	std::string name;
	Int32 value = 0;
};

/// A labelled spot on a slider, like "60 W bulb" at 800 lm.
struct PropertyMark {
	F32 value = 0.F;
	std::string_view label{};
};

/// How the inspector shows and limits a property. Every field is optional.
///
/// - speed: drag speed of number fields
/// - precision: decimals shown
/// - color: Vec3/Vec4 get a color picker
/// - toggle: bool as a switch, not a checkbox
/// - slider: draws a fill bar between min and max
/// - unit: suffix like "m"
/// - asset_type: the string is an asset path, the inspector shows an asset slot for that type
///
/// Floats with `logarithmic`, `marks` or `gradient` get a slider with a typed value next to it instead:
/// - logarithmic: every doubling is the same distance, for values that span decades (light intensities)
/// - slider_min/slider_max: the slider's range when it's narrower than min/max, typing can still go past it.
///   Must be above 0 when logarithmic.
/// - marks: reference points drawn on the track, the nearest one is named under it
/// - gradient: colors spread evenly along the track, like a temperature scale
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
	bool logarithmic = false;
	F32 slider_min = 0.F;
	F32 slider_max = 0.F;
	std::span<const PropertyMark> marks{};
	std::span<const Vec4> gradient{};
};

/// One reflected field. The instance is a type-erased pointer to the object.
/// An empty `visible` means always visible.
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
