#include "Aquila/UI/Core/Behaviour.h"

#include "Aquila/Foundation/Log.h"
#include "Aquila/UI/Core/LayoutLoader.h"

namespace Aquila::UI::Core {

Behaviour::Behaviour() = default;

Behaviour::~Behaviour() {
	run_cleanups();
}

void Behaviour::run_cleanups() {
	std::vector<Delegate<void()>> cleanups = std::exchange(m_cleanups, {});
	for (auto it = cleanups.rbegin(); it != cleanups.rend(); ++it) {
		if (*it) {
			(*it)();
		}
	}
}

View &Behaviour::root() const {
	AQUILA_ASSERT(m_instance != nullptr, "Behaviour: root() before the behaviour was attached to a component");
	return *m_instance->m_root;
}

std::string_view Behaviour::attribute(std::string_view name) const {
	if (m_instance == nullptr) {
		return {};
	}
	const auto it = m_instance->m_attributes.find(name);
	return it != m_instance->m_attributes.end() ? std::string_view(it->second) : std::string_view();
}

View *Behaviour::create(std::string_view component, View &parent, ComponentArgs args) {
	Unique<View> created = create_detached(component, std::move(args));
	return created ? parent.add_child(std::move(created)) : nullptr;
}

Unique<View> Behaviour::create_detached(std::string_view component, ComponentArgs args) {
	if (m_instance == nullptr || m_instance->m_loader == nullptr) {
		return nullptr;
	}
	return m_instance->m_loader->create_detached_from(*m_instance, component, std::move(args));
}

void Behaviour::add_cleanup(Delegate<void()> cleanup) {
	m_cleanups.push_back(std::move(cleanup));
}

View *Behaviour::find_view(std::string_view id) const {
	if (id.empty() || m_instance == nullptr || m_instance->m_root == nullptr) {
		return nullptr;
	}
	const Uint64 scope = m_instance->get_scope_id();
	std::vector<View *> pending{ m_instance->m_root };
	while (!pending.empty()) {
		View *view = pending.back();
		pending.pop_back();
		if (view->get_id_scope() == scope && view->get_id() == id) {
			return view;
		}
		for (auto it = view->get_children().rbegin(); it != view->get_children().rend(); ++it) {
			pending.push_back(it->get());
		}
	}
	AQUILA_LOG_ERROR("'{}': no element '{}' ({})", m_instance->get_name(), id, m_instance->get_file());
	return nullptr;
}

void Behaviour::report_wrong_type(std::string_view id, const View &found) const {
	AQUILA_LOG_ERROR("'{}': '{}' is a {}, not what was asked for ({})", m_instance->get_name(), id,
					 found.get_type_name(), m_instance->get_file());
}

void Behaviour::report_missing_behaviour(std::string_view component) const {
	AQUILA_LOG_ERROR("'{}': '{}' doesn't have the behaviour that was asked for ({})", m_instance->get_name(), component,
					 m_instance->get_file());
}

}
