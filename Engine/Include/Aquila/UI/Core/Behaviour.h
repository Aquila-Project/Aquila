#ifndef AQUILA_UI_CORE_BEHAVIOUR_H
#define AQUILA_UI_CORE_BEHAVIOUR_H

#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/Signal.h"
#include "Aquila/UI/Core/ComponentInstance.h"
#include "Aquila/UI/Core/View.h"

namespace Aquila::UI::Core {

class Behaviour {
  public:
	Behaviour();
	virtual ~Behaviour();

	AQUILA_NONCOPYABLE(Behaviour);
	AQUILA_NONMOVEABLE(Behaviour);

	virtual void define_behaviour() {}

	[[nodiscard]] View &root() const;
	[[nodiscard]] std::string_view attribute(std::string_view name) const;

	template <std::derived_from<View> T = View> [[nodiscard]] T *get(std::string_view id) const {
		View *view = find_view(id);
		if (view == nullptr) {
			return nullptr;
		}
		auto *typed = dynamic_cast<T *>(view);
		if (typed == nullptr) {
			report_wrong_type(id, *view);
		}
		return typed;
	}

	template <typename T> [[nodiscard]] T *get_component(std::string_view id) const {
		View *view = find_view(id);
		if (view == nullptr) {
			return nullptr;
		}
		for (ComponentInstance *instance = view->get_component_instance(); instance != nullptr;
			 instance = instance->get_inner()) {
			if (auto *typed = dynamic_cast<T *>(instance->get_behaviour())) {
				return typed;
			}
		}
		report_wrong_type(id, *view);
		return nullptr;
	}

	View *create(std::string_view component, View &parent);

	template <std::derived_from<Behaviour> T> T *create(std::string_view component, View &parent) {
		View *view = create(component, parent);
		if (view == nullptr) {
			return nullptr;
		}
		for (ComponentInstance *instance = view->get_component_instance(); instance != nullptr;
			 instance = instance->get_inner()) {
			if (auto *typed = dynamic_cast<T *>(instance->get_behaviour())) {
				return typed;
			}
		}
		report_missing_behaviour(component);
		parent.remove_child(view);
		return nullptr;
	}

  protected:
	template <typename... Args, typename Slot> void listen(Signal<void(Args...)> &signal, Slot slot) {
		auto connection = signal.connect(std::move(slot));
		add_cleanup([&signal, connection] { signal.disconnect(connection); });
	}

	void add_cleanup(Delegate<void()> cleanup);
	void run_cleanups();

  private:
	friend class LayoutLoader;

	[[nodiscard]] View *find_view(std::string_view id) const;
	void report_wrong_type(std::string_view id, const View &found) const;
	void report_missing_behaviour(std::string_view component) const;

	ComponentInstance *m_instance = nullptr;
	std::vector<Delegate<void()>> m_cleanups;
};

}

#endif
