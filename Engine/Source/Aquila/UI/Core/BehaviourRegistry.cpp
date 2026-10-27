#include "Aquila/UI/Core/BehaviourRegistry.h"

#include "Aquila/Foundation/Log.h"

namespace Aquila::UI::Core {

BehaviourRegistry::BehaviourRegistry() = default;
BehaviourRegistry::~BehaviourRegistry() = default;

Foundation::Registration BehaviourRegistry::add(std::string name, Factory factory) {
	if (m_factories.contains(name)) {
		AQUILA_LOG_WARNING("BehaviourRegistry: '{}' is already registered", name);
		return {};
	}
	m_factories.emplace(name, std::move(factory));
	return m_owner.make([this, name = std::move(name)] { m_factories.erase(name); });
}

Unique<Behaviour> BehaviourRegistry::create(std::string_view name) const {
	const auto it = m_factories.find(name);
	if (it == m_factories.end() || !it->second) {
		return nullptr;
	}
	return it->second();
}

}
