#ifndef AQUILA_EDITOR_BUILD_WINDOW_H
#define AQUILA_EDITOR_BUILD_WINDOW_H

#include "Aquila/Foundation/Defines.h"
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Platform/Cursor.h"
#include "Aquila/Player/BuildPlan.h"
#include "Aquila/Player/BuildRunner.h"

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
class DragInt;
class Dropdown;
class Label;
class ScrollView;
class SelectableTextView;
class TextInput;
class View;
}

namespace Editor {

class EditorContext;

/// UI around plan_build() and run_build(). Builds run on a worker thread, progress is queued under
/// a mutex and shown in update().
class BuildWindow {
  public:
	explicit BuildWindow(EditorContext &context);
	~BuildWindow();

	AQUILA_NONCOPYABLE(BuildWindow);
	AQUILA_NONMOVEABLE(BuildWindow);

	void build(Uint32 width, Uint32 height, const std::vector<std::string> &style_paths);

	void update(F32 delta_time);
	void render(Aquila::Graphics::Renderer2D &batcher, Aquila::GFX::GfxCommandList &cmd);
	void on_event(Aquila::Platform::Events::Event &event);
	void set_frame_target(const void *target);
	[[nodiscard]] Aquila::Platform::CursorType get_cursor() const;

	/// With `run_after` the game starts once the build succeeds.
	void start_build(bool run_after);
	[[nodiscard]] bool is_busy() const { return m_busy; }

	Delegate<void()> on_request_close;

  private:
	enum class LineKind : Uint8 { Info, Step, Warning, Error, Success };
	enum class StatusKind : Uint8 { Idle, Busy, Error, Success };

	struct Message {
		LineKind kind = LineKind::Info;
		std::string text;
		bool finished = false;
		Aquila::Player::BuildResult result{};
	};

	void build_settings(Aquila::UI::Core::View &content);
	void wire_buttons(Aquila::UI::Core::View &root);
	void preview();
	void finish(const Aquila::Player::BuildResult &result);
	[[nodiscard]] Aquila::Player::BuildRequest make_request() const;
	/// Writes the scene, output, size and icon picked here into the project settings, so they stick.
	void remember_choices();
	Option<Aquila::Player::BuildPlan> plan(bool show_steps);
	void refresh_template();
	void set_busy(bool busy);
	void set_status(StatusKind kind, std::string text);
	void clear_log();
	void log(LineKind kind, std::string text);
	void post(Message message);
	[[nodiscard]] bool is_build_chord(Aquila::Platform::Events::Event &event) const;

	EditorContext &m_context;
	Unique<Aquila::UI::Core::Canvas> m_canvas;

	Aquila::UI::Core::TextInput *m_output = nullptr;
	Aquila::UI::Core::Dropdown *m_scene = nullptr;
	Aquila::UI::Core::DragInt *m_width = nullptr;
	Aquila::UI::Core::DragInt *m_height = nullptr;
	Aquila::UI::Core::Dropdown *m_icon = nullptr;
	Aquila::UI::Core::Label *m_template_state = nullptr;
	Aquila::UI::Core::SelectableTextView *m_template_path = nullptr;
	Aquila::UI::Core::Label *m_status = nullptr;
	Aquila::UI::Core::ScrollView *m_log_scroll = nullptr;
	Aquila::UI::Core::SelectableTextView *m_log = nullptr;
	std::array<Aquila::UI::Core::Button *, 3> m_buttons{};

	std::thread m_worker;
	std::mutex m_mutex;
	std::vector<Message> m_inbox;
	bool m_busy = false;
	bool m_run_after = false;
};

}

#endif
