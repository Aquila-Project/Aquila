#ifndef AQUILA_FOUNDATION_REFLECTION_TYPE_INFO_H
#define AQUILA_FOUNDATION_REFLECTION_TYPE_INFO_H

#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/Reflection/Property.h"

namespace Aquila::Reflection {

/// Hand-written reflection for one type, filled with TypeBuilder.
class TypeInfo {
  public:
	explicit TypeInfo(std::string name) : m_name(std::move(name)) {}

	[[nodiscard]] const std::string &get_name() const { return m_name; }
	[[nodiscard]] const std::vector<Property> &get_properties() const { return m_properties; }

	[[nodiscard]] const Property *find(std::string_view name) const {
		for (const auto &property : m_properties) {
			if (property.name == name) {
				return &property;
			}
		}
		return nullptr;
	}

	Property &add(Property property) { return m_properties.emplace_back(std::move(property)); }

  private:
	std::string m_name;
	std::vector<Property> m_properties;
};

/// options() and visible_if() apply to the property added right before them.
///
/// @code
/// struct Fog {
///     bool enabled = true;
///     F32 density = 0.1F;
/// };
///
/// TypeInfo info("Fog");
/// TypeBuilder<Fog>(info)
///     .property("Enabled", &Fog::enabled)
///     .property("Density", &Fog::density, { .min = 0.F, .max = 1.F, .slider = true })
///     .visible_if([](const Fog &fog) { return fog.enabled; });
/// @endcode
template <typename T> class TypeBuilder {
  public:
	explicit TypeBuilder(TypeInfo &info) : m_info(info) {}

	/// `access` is a member pointer or anything that returns a reference to the field.
	template <typename Access> TypeBuilder &property(std::string name, Access access, PropertyHints hints = {}) {
		using V = std::decay_t<std::invoke_result_t<Access, T &>>;
		return add<V>(
			std::move(name), hints,
			[access](const void *instance) {
				return detail::to_value<V>(std::invoke(access, *const_cast<T *>(static_cast<const T *>(instance))));
			},
			[access](void *instance, const PropertyValue &value) {
				std::invoke(access, *static_cast<T *>(instance)) = detail::from_value<V>(value);
			});
	}

	template <typename Getter, typename Setter>
		requires std::is_invocable_v<Setter, T &, std::decay_t<std::invoke_result_t<Getter, const T &>>>
	TypeBuilder &property(std::string name, Getter getter, Setter setter, PropertyHints hints = {}) {
		using V = std::decay_t<std::invoke_result_t<Getter, const T &>>;
		return add<V>(
			std::move(name), hints,
			[getter](const void *instance) {
				return detail::to_value<V>(std::invoke(getter, *static_cast<const T *>(instance)));
			},
			[setter](void *instance, const PropertyValue &value) {
				std::invoke(setter, *static_cast<T *>(instance), detail::from_value<V>(value));
			});
	}

	/// Edits are ignored, the inspector shows it as a label.
	template <typename Getter> TypeBuilder &read_only(std::string name, Getter getter, PropertyHints hints = {}) {
		using V = std::decay_t<std::invoke_result_t<Getter, const T &>>;
		hints.read_only = true;
		return add<V>(
			std::move(name), hints,
			[getter](const void *instance) {
				return detail::to_value<V>(std::invoke(getter, *static_cast<const T *>(instance)));
			},
			[](void *, const PropertyValue &) {});
	}

	TypeBuilder &options(std::vector<EnumOption> options) {
		last().options = std::move(options);
		return *this;
	}

	TypeBuilder &visible_if(Delegate<bool(const T &)> predicate) {
		last().visible = [predicate = std::move(predicate)](const void *instance) {
			return predicate(*static_cast<const T *>(instance));
		};
		return *this;
	}

  private:
	template <typename V, typename Get, typename Set>
	TypeBuilder &add(std::string name, PropertyHints hints, Get get, Set set) {
		Property property;
		property.name = std::move(name);
		property.kind = detail::kind_of<V>();
		property.hints = hints;
		property.get = std::move(get);
		property.set = std::move(set);
		m_last = &m_info.add(std::move(property));
		return *this;
	}

	Property &last() {
		AQUILA_ASSERT(m_last != nullptr, "TypeBuilder: options()/visible_if() need a property before them");
		return *m_last;
	}

	TypeInfo &m_info;
	Property *m_last = nullptr;
};

}

#endif
