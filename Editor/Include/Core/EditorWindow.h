#pragma once

#include "Aquila/Foundation/Defines.h"
#include "Aquila/Foundation/Signal.h"
#include "Aquila/UI/Core/View.h"
#include "Aquila/UI/Widgets/DockPanel.h"
#include "Core/EditorContext.h"

namespace Editor {

/// Every editor panel. build() fills it once. Its dock panel owns it, so it's destroyed when the tab
/// closes, the workspace switches or it gets torn off, and everything it hooked up goes with it.
///
/// @code
/// class StatsWindow final : public EditorWindow {
///   public:
///     using EditorWindow::EditorWindow;
///     void build(View &content) override {
///         m_label = content.add_child<Label>("0 entities");
///         listen(context().selection.on_entity_created, [this](Entity) { refresh(); });
///     }
/// };
///
/// module.add_window<StatsWindow>({ .id = "stats", .title = "Stats", .icon = "chart-bar" });
/// @endcode
class EditorWindow : public Aquila::UI::Core::DockPanelContent {
  public:
	explicit EditorWindow(EditorContext &context) : m_context(context) {}
	~EditorWindow() override;

	AQUILA_NONCOPYABLE(EditorWindow);
	AQUILA_NONMOVEABLE(EditorWindow);

	virtual void build(Aquila::UI::Core::View &content) = 0;
	/// Every frame while it's open.
	virtual void update(F32 delta_time) {}
	/// Whether this window's scoped shortcuts run right now. By default while the mouse is over it.
	[[nodiscard]] virtual bool accepts_shortcuts(bool hovered) const { return hovered; }

	[[nodiscard]] EditorContext &context() const { return m_context; }
	[[nodiscard]] const std::string &get_type_id() const { return m_type_id; }

  protected:
	/// Connects now, disconnects when the window goes away.
	template <typename... Args, typename Slot> void listen(Signal<void(Args...)> &signal, Slot slot) {
		auto connection = signal.connect(std::move(slot));
		m_disconnects.emplace_back([&signal, connection] { signal.disconnect(connection); });
	}

	/// The command exists while the window is open.
	void add_command(Aquila::UI::Core::Command command) {
		std::string id = command.id;
		m_context.shortcuts.add_command(std::move(command));
		m_disconnects.emplace_back([this, id = std::move(id)] { m_context.shortcuts.remove_command(id); });
	}

	[[nodiscard]] Unique<Aquila::UI::Core::View> load_layout(std::string_view file_name) const {
		return m_context.load_layout(file_name);
	}

	/// Popups added under it during build() are moved into the panel, so they go away with the window.
	[[nodiscard]] Aquila::UI::Core::View *overlay_root() const { return m_context.overlay_root(); }

  private:
	friend class EditorWindows;

	void release();

	EditorContext &m_context;
	std::string m_type_id;
	std::vector<Delegate<void()>> m_disconnects;
	std::vector<Aquila::UI::Core::ViewRef> m_overlays;
	Delegate<void(EditorWindow *)> m_on_destroyed;
	bool m_released = false;
};

}
