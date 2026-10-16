#ifndef AQUILA_SCENE_COMPONENT_REGISTRY_H
#define AQUILA_SCENE_COMPONENT_REGISTRY_H

#include "Aquila/Foundation/Reflection/TypeInfo.h"
#include "Aquila/Foundation/Signal.h"
#include "Aquila/Scene/Entity.h"

namespace Aquila::SceneManagement {

enum class ComponentCategory : Uint8 { Object, General, Rendering };

class ComponentDescriptor {
  public:
	explicit ComponentDescriptor(std::string name) : m_type(std::move(name)) {}

	[[nodiscard]] const std::string &get_name() const { return m_type.get_name(); }
	[[nodiscard]] const Reflection::TypeInfo &get_type() const { return m_type; }

	[[nodiscard]] bool has(Entity entity) const { return m_has(entity); }
	[[nodiscard]] bool is_removable() const { return m_removable; }
	[[nodiscard]] ComponentCategory get_category() const { return m_category; }
	void remove(Entity entity) const { m_remove(entity); }
	[[nodiscard]] bool can_add() const { return static_cast<bool>(m_add); }
	void add(Entity entity) const {
		if (m_add) {
			m_add(entity);
		}
	}
	[[nodiscard]] void *get(Entity entity) const { return m_get(entity); }
	[[nodiscard]] Signal<void()> *get_changed_signal(Entity entity) const { return m_get_changed(entity); }

  private:
	friend class ComponentRegistry;

	Reflection::TypeInfo m_type;
	bool m_removable = true;
	ComponentCategory m_category = ComponentCategory::General;
	Delegate<bool(Entity)> m_has;
	Delegate<void(Entity)> m_remove;
	Delegate<void(Entity)> m_add;
	Delegate<void *(Entity)> m_get;
	Delegate<Signal<void()> *(Entity)> m_get_changed;
};

class ComponentRegistry {
  public:
	[[nodiscard]] static ComponentRegistry &instance();

	template <typename T>
	Reflection::TypeBuilder<T> register_component(std::string name, bool removable = true,
												  ComponentCategory category = ComponentCategory::General) {
		auto descriptor = std::make_unique<ComponentDescriptor>(std::move(name));
		descriptor->m_removable = removable;
		descriptor->m_category = category;
		descriptor->m_has = [](Entity entity) { return entity.has_component<T>(); };
		descriptor->m_remove = [](Entity entity) { entity.try_remove_component<T>(); };
		if constexpr (std::is_default_constructible_v<T>) {
			descriptor->m_add = [](Entity entity) { entity.get_or_emplace<T>(); };
		}
		descriptor->m_get = [](Entity entity) -> void * { return entity.try_get_component<T>(); };
		descriptor->m_get_changed = [](Entity entity) -> Signal<void()> * {
			if constexpr (requires(T &component) {
							  { component.on_changed } -> std::convertible_to<Signal<void()> &>;
						  }) {
				T *component = entity.try_get_component<T>();
				return component != nullptr ? &component->on_changed : nullptr;
			} else {
				return nullptr;
			}
		};

		Reflection::TypeInfo &type = descriptor->m_type;
		m_descriptors.push_back(std::move(descriptor));
		return Reflection::TypeBuilder<T>(type);
	}

	[[nodiscard]] const ComponentDescriptor *find(std::string_view name) const;
	[[nodiscard]] const std::vector<Unique<ComponentDescriptor>> &get_all() const { return m_descriptors; }

  private:
	ComponentRegistry();

	std::vector<Unique<ComponentDescriptor>> m_descriptors;
};

}

#endif
