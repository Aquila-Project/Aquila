#pragma once

#include "Aquila/Rendering/RenderPipeline.h"
#include "Aquila/Rendering/RenderView.h"
#include "Aquila/Scene/SceneStatistics.h"
#include "UI/Viewport/ViewportCanvas.h"

namespace Editor {

class EditorContext;

/// Axis widget, selection label, safe frame and the debug panel over the viewport. Never hit tested.
class ViewportOverlay final : public ViewportCanvas {
  public:
	struct Options {
		bool axis_widget = true;
		bool selection_label = true;
		bool safe_frame = false;
		bool render_status = false;
		bool statistics = false;
		bool gpu_timings = false;
	};

	struct DebugInfo {
		std::string shading;
		Uint32 width = 0;
		Uint32 height = 0;
		F32 cpu_ms = 0.F;
		F32 gpu_ms = 0.F;
		bool has_gpu = false;
		Aquila::SceneManagement::SceneStatistics statistics;
		std::vector<Aquila::Rendering::RenderPipeline::PassTiming> passes;
	};

	explicit ViewportOverlay(EditorContext &context);

	[[nodiscard]] std::string_view get_type_name() const override { return "ViewportOverlay"; }

	/// Call it every frame. It compares and only redraws when something it shows changed.
	void sync(const Rect &viewport, const Aquila::Rendering::RenderView &view);
	[[nodiscard]] const Options &options() const { return m_options; }
	void set_option(bool Options::*flag, bool enabled);
	[[nodiscard]] bool wants_debug_info() const {
		return m_options.render_status || m_options.statistics || m_options.gpu_timings;
	}
	void set_debug_info(DebugInfo info);

	void draw(Aquila::UI::Rendering::DrawList &draw_list) const override;

  private:
	void draw_axis_widget(Aquila::UI::Rendering::DrawList &draw_list) const;
	void draw_selection_label(Aquila::UI::Rendering::DrawList &draw_list) const;
	void draw_safe_frame(Aquila::UI::Rendering::DrawList &draw_list) const;
	void draw_debug_panel(Aquila::UI::Rendering::DrawList &draw_list) const;
	void draw_centered_text(Aquila::UI::Rendering::DrawList &draw_list, Vec2 center, const std::string &text,
							F32 size, Vec4 color, Int32 z) const;

	EditorContext &m_context;
	Options m_options;
	Rect m_viewport;
	Aquila::Rendering::RenderView m_view;
	std::string m_label;
	Option<Vec2> m_label_anchor;
	DebugInfo m_debug;
};

}
