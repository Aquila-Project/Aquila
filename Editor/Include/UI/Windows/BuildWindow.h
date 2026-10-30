#ifndef AQUILA_EDITOR_BUILD_WINDOW_H
#define AQUILA_EDITOR_BUILD_WINDOW_H

#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Foundation/Reflection/TypeInfo.h"
#include "Aquila/Foundation/Signal.h"
#include "Aquila/Player/BuildPlan.h"
#include "Aquila/Player/BuildRunner.h"
#include "Aquila/UI/Core/WindowBehaviour.h"
#include "Core/ProjectSettings.h"

namespace Aquila::Platform::Events {
class Event;
}

namespace Aquila::UI::Core {
class Button;
class Label;
class PropertyGrid;
class ReflectedPropertyGrid;
class ScrollView;
class SelectableTextView;
class View;
}

namespace Editor {

class BuildTemplateInfo;
class EditorContext;

/// UI around plan_build() and run_build(). Builds run on a worker thread, progress is queued under
/// a mutex and shown in on_update().
class BuildWindow final : public Aquila::UI::Core::WindowBehaviour {
  public:
	explicit BuildWindow(EditorContext &context);
	~BuildWindow() override;

	void define_behaviour() override;
	void update(F32 delta_time) override;
	bool intercept_event(Aquila::Platform::Events::Event &event) override;

	/// With `run_after` the game starts once the build succeeds.
	void start_build(bool run_after);
	[[nodiscard]] bool is_busy() const { return m_busy; }

  private:
	enum class LineKind : Uint8 { Info, Step, Warning, Error, Success };
	enum class StatusKind : Uint8 { Idle, Busy, Error, Success };

	struct Message {
		LineKind kind = LineKind::Info;
		std::string text;
		bool finished = false;
		Aquila::Player::BuildResult result{};
	};

	void build_settings();
	void pick_icon();
	void wire_buttons(Aquila::UI::Core::View &root);
	void preview();
	void finish(const Aquila::Player::BuildResult &result);
	[[nodiscard]] Aquila::Player::BuildRequest make_request() const;
	Option<Aquila::Player::BuildPlan> plan(bool show_steps);
	void refresh_template();
	void set_busy(bool busy);
	void set_status(StatusKind kind, std::string text);
	void clear_log();
	void log(LineKind kind, std::string text);
	void post(Message message);
	[[nodiscard]] bool is_build_chord(Aquila::Platform::Events::Event &event) const;

	EditorContext &m_context;

	ProjectSettings m_settings;
	Unique<Aquila::Reflection::TypeInfo> m_settings_type;
	Unique<Aquila::UI::Core::ReflectedPropertyGrid> m_fields;
	Signal<void()>::Connection m_settings_connection;
	Aquila::UI::Core::PropertyGrid *m_grid = nullptr;
	bool m_rebuild_settings = false;
	BuildTemplateInfo *m_template = nullptr;
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
