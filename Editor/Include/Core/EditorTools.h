#pragma once

#include "Aquila/Foundation/Signal.h"

namespace Editor {

enum class TransformTool { Translate, Rotate, Scale };
enum class TransformSpace { World, Local };

/// The transform tool state shared by the viewport toolbar, the gizmo and the shortcuts.
/// Snapping is 0.5 m, 15 degrees and 0.1 scale. Holding Ctrl while dragging flips it.
class EditorTools {
  public:
	void set_tool(TransformTool tool) {
		if (m_tool == tool) {
			return;
		}
		m_tool = tool;
		on_tool_changed(tool);
	}

	void set_space(TransformSpace space) {
		if (m_space == space) {
			return;
		}
		m_space = space;
		on_space_changed(space);
	}

	void toggle_space() { set_space(m_space == TransformSpace::World ? TransformSpace::Local : TransformSpace::World); }

	void set_snapping(bool snapping) {
		if (m_snapping == snapping) {
			return;
		}
		m_snapping = snapping;
		on_snapping_changed(snapping);
	}

	void toggle_snapping() { set_snapping(!m_snapping); }

	[[nodiscard]] TransformTool get_tool() const { return m_tool; }
	[[nodiscard]] TransformSpace get_space() const { return m_space; }
	[[nodiscard]] bool is_snapping() const { return m_snapping; }
	[[nodiscard]] F32 get_translate_snap() const { return m_translate_snap; }
	[[nodiscard]] F32 get_rotate_snap_degrees() const { return m_rotate_snap_degrees; }
	[[nodiscard]] F32 get_scale_snap() const { return m_scale_snap; }

	Signal<void(TransformTool)> on_tool_changed;
	Signal<void(TransformSpace)> on_space_changed;
	Signal<void(bool)> on_snapping_changed;

  private:
	TransformTool m_tool = TransformTool::Translate;
	TransformSpace m_space = TransformSpace::World;
	bool m_snapping = false;
	F32 m_translate_snap = 0.5F;
	F32 m_rotate_snap_degrees = 15.F;
	F32 m_scale_snap = 0.1F;
};

} // namespace Editor
