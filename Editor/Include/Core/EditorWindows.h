#pragma once

#include "Aquila/Foundation/Defines.h"
#include "Aquila/UI/Core/DockLayoutSerializer.h"
#include "Core/EditorWindow.h"

namespace Aquila::UI::Core {
class DockNode;
class DockPanel;
class DockSpace;
class PopupMenu;
class View;
}

namespace Editor {

/// `id` is also the dock panel id, so content ids inside the window must not reuse it.
/// Windows that aren't single instance get panel ids like "id#2".
/// `group` is the section of the editor switcher menu.
struct EditorWindowType {
	std::string id;
	std::string title;
	std::string icon = "square";
	std::string group = "General";
	bool single_instance = true;
	Delegate<Unique<EditorWindow>(EditorContext &)> create{};
};

/// The window types and the open windows. Plugs into the DockSpace as its panel factory,
/// tab bar decorator (editor switcher and +) and closable tabs.
class EditorWindows {
  public:
	explicit EditorWindows(EditorContext &context);
	~EditorWindows();

	AQUILA_NONCOPYABLE(EditorWindows);
	AQUILA_NONMOVEABLE(EditorWindows);

	template <typename T> void add(EditorWindowType type) {
		type.create = [](EditorContext &context) -> Unique<EditorWindow> { return std::make_unique<T>(context); };
		add(std::move(type));
	}
	void add(EditorWindowType type);

	void attach(Aquila::UI::Core::DockSpace &dock_space);
	void update(F32 delta_time);
	void shutdown();

	/// A single instance window that's already open is moved to `target` and focused instead.
	EditorWindow *open(std::string_view type_id, Aquila::UI::Core::DockNode *target = nullptr);
	/// Swaps the active tab of `node` for another window type, what the editor switcher does.
	void replace(Aquila::UI::Core::DockNode *node, std::string_view type_id);
	void apply_layout(const Aquila::UI::Core::DockLayoutDesc &layout);
	[[nodiscard]] Aquila::UI::Core::DockLayoutDesc capture() const;

	[[nodiscard]] const std::vector<EditorWindowType> &get_types() const { return m_types; }
	[[nodiscard]] const EditorWindowType *find_type(std::string_view type_id) const;
	[[nodiscard]] bool is_open(std::string_view type_id) const;
	/// Shortcut scopes are window type ids. Active when that window is visible and accepts shortcuts.
	[[nodiscard]] bool is_scope_active(std::string_view type_id) const;
	/// A window of this type is open and on screen: its tab is the active one, nothing above it is hidden.
	/// Follows the computed style, so it lags one frame behind a tab switch.
	[[nodiscard]] bool is_visible(std::string_view type_id) const;

	template <typename T> [[nodiscard]] T *find() const {
		for (const Live &live : m_live) {
			if (auto *window = dynamic_cast<T *>(live.window)) {
				return window;
			}
		}
		return nullptr;
	}

  private:
	struct Live {
		EditorWindow *window = nullptr;
		Aquila::UI::Core::DockPanel *panel = nullptr;
	};

	Unique<Aquila::UI::Core::DockPanel> create_panel(const std::string &panel_id);
	void decorate(Aquila::UI::Core::DockNode *node, Aquila::UI::Core::View *actions);
	void open_type_menu(Vec2 position, bool replace, Aquila::UI::Core::DockNode *node);
	[[nodiscard]] const Live *find_live(std::string_view type_id) const;
	[[nodiscard]] Aquila::UI::Core::DockNode *default_target() const;
	void forget(EditorWindow *window);

	EditorContext &m_context;
	Aquila::UI::Core::DockSpace *m_dock_space = nullptr;
	Aquila::UI::Core::PopupMenu *m_menu = nullptr;
	std::vector<EditorWindowType> m_types;
	std::vector<Live> m_live;
	Uint32 m_next_instance = 1;
	bool m_shut_down = false;
};

}
