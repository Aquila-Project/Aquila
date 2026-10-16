#pragma once

#include "Aquila/Foundation/Defines.h"
#include "Aquila/Foundation/Signal.h"
#include "Aquila/UI/Core/DockLayoutSerializer.h"

namespace Aquila::UI::Core {
class Button;
class View;
}

namespace Editor {

class EditorWindows;

class Workspaces {
  public:
	explicit Workspaces(EditorWindows &windows) : m_windows(windows) {}

	AQUILA_NONCOPYABLE(Workspaces);
	AQUILA_NONMOVEABLE(Workspaces);

	void add(std::string name, Aquila::UI::Core::DockLayoutDesc preset);
	void build_tabs(Aquila::UI::Core::View &container);

	void activate(std::string_view name);
	void reset_active();

	[[nodiscard]] const std::string &get_active() const { return m_active; }

	Signal<void(const std::string &)> on_activated;

  private:
	struct Entry {
		std::string name;
		Aquila::UI::Core::DockLayoutDesc preset;
		Option<Aquila::UI::Core::DockLayoutDesc> current{};
		Aquila::UI::Core::Button *tab = nullptr;
	};

	Entry *find(std::string_view name);
	void create_tab(Entry &entry);
	void refresh_tabs();

	EditorWindows &m_windows;
	std::vector<Entry> m_entries;
	std::string m_active;
	Aquila::UI::Core::View *m_tab_container = nullptr;
};

}
