#pragma once

#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Foundation/Reflection/TypeInfo.h"
#include "Aquila/Foundation/Signal.h"
#include "Core/ProjectSettings.h"

namespace Aquila::Graphics {
class Renderer2D;
}
namespace Aquila::GFX {
class GfxCommandList;
}
namespace Aquila::Platform::Events {
class Event;
}
namespace Aquila::UI::Core {
class Button;
class Canvas;
class Label;
class ReflectedPropertyGrid;
class TextInput;
class View;
} // namespace Aquila::UI::Core

namespace Editor {

class EditorContext;

class SettingsWindow {
  public:
	explicit SettingsWindow(EditorContext &context);
	~SettingsWindow();

	void build(Uint32 width, Uint32 height, const std::vector<std::string> &style_paths);
	void update(F32 delta_time);
	void render(Aquila::Graphics::Renderer2D &batcher, Aquila::GFX::GfxCommandList &cmd);
	void on_event(Aquila::Platform::Events::Event &event);
	void set_frame_target(const void *target);

	Delegate<void()> on_request_close;
	Delegate<void()> on_preferences_changed;

  private:
	enum class Scope : Uint8 { Editor, Project };

	struct Page {
		Scope scope = Scope::Editor;
		std::string name;
		std::string icon;
		std::string description;
		Unique<Aquila::Reflection::TypeInfo> type;
		void *instance = nullptr;
		Delegate<void()> reset;
		bool shortcuts = false;
	};

	Page &add_page(Scope scope, std::string name, std::string icon, std::string description);
	void add_editor_pages();
	void add_project_pages();
	void build_header(Aquila::UI::Core::View &root);
	void build_footer(Aquila::UI::Core::View &root);

	void select_scope(Scope scope);
	void select_page(Usize index);
	void request_rebuild();
	void rebuild();
	void rebuild_sidebar();
	void show_page(Page &page, const std::string &query, bool full);
	void show_shortcuts(Aquila::UI::Core::View &host, const std::string &query);
	void edited(const Page &page);

	EditorContext &m_context;
	Unique<Aquila::UI::Core::Canvas> m_canvas;
	ProjectSettings m_project;
	std::vector<std::string> m_font_families;
	std::vector<std::string> m_icons;
	std::vector<Page> m_pages;
	Scope m_scope = Scope::Editor;
	std::array<Usize, 2> m_selected{};
	std::string m_query;
	Aquila::UI::Core::View *m_sidebar = nullptr;
	Aquila::UI::Core::View *m_content = nullptr;
	Aquila::UI::Core::Label *m_footer_note = nullptr;
	Aquila::UI::Core::TextInput *m_search = nullptr;
	std::array<Aquila::UI::Core::Button *, 2> m_scope_tabs{};
	std::vector<Unique<Aquila::Reflection::TypeInfo>> m_filtered_types;
	std::vector<Unique<Aquila::UI::Core::ReflectedPropertyGrid>> m_grids;
	Signal<void()>::Connection m_settings_connection;
	bool m_rebuild_pending = false;
};

} // namespace Editor
