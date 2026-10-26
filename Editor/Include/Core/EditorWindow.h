#pragma once

#include "Aquila/Foundation/Defines.h"
#include "Aquila/Foundation/Signal.h"
#include "Aquila/UI/Core/View.h"
#include "Aquila/UI/Core/WindowBehaviour.h"
#include "Aquila/UI/Widgets/DockPanel.h"
#include "Core/EditorContext.h"

namespace Editor {

class EditorWindow : public Aquila::UI::Core::DockPanelContent, public Aquila::UI::Core::WindowBehaviour {
  public:
	explicit EditorWindow(EditorContext &context, std::string layout_path = {})
		: WindowBehaviour(std::move(layout_path)), m_context(context) {}
	~EditorWindow() override;

	AQUILA_NONCOPYABLE(EditorWindow);
	AQUILA_NONMOVEABLE(EditorWindow);

	virtual void build(Aquila::UI::Core::View &content) {}
	/// Whether this window's scoped shortcuts run right now. By default while the mouse is over it.
	[[nodiscard]] virtual bool accepts_shortcuts(bool hovered) const { return hovered; }

	[[nodiscard]] EditorContext &context() const { return m_context; }
	[[nodiscard]] const std::string &get_type_id() const { return m_type_id; }

  protected:
	/// The command exists while the window is open.
	void add_command(Aquila::UI::Core::Command command) {
		std::string id = command.id;
		m_context.commands.add(std::move(command));
		add_cleanup([this, id = std::move(id)] { m_context.commands.remove(id); });
	}

	[[nodiscard]] Unique<Aquila::UI::Core::View> load_layout(std::string_view file_name) const {
		return m_context.load_layout(file_name);
	}


  private:
	friend class EditorWindows;

	void release();

	EditorContext &m_context;
	std::string m_type_id;
	std::vector<Aquila::UI::Core::ViewRef> m_overlays;
	Delegate<void(EditorWindow *)> m_on_destroyed;
	bool m_released = false;
};
}
