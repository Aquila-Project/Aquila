#include "Core/DefaultShortcuts.h"

#include "Aquila/Foundation/Log.h"
#include "Aquila/UI/Core/Shortcuts.h"

namespace Editor {

namespace {

struct DefaultShortcut {
	std::string_view command;
	std::string_view chord;
	std::string_view scope{};
};

constexpr std::array k_default_shortcuts{
	DefaultShortcut{ .command = "scene.new", .chord = "Ctrl+N" },
	DefaultShortcut{ .command = "scene.open", .chord = "Ctrl+O" },
	DefaultShortcut{ .command = "scene.save", .chord = "Ctrl+S" },
	DefaultShortcut{ .command = "scene.save_as", .chord = "Ctrl+Shift+S" },
	DefaultShortcut{ .command = "editor.preferences", .chord = "Ctrl+," },
	DefaultShortcut{ .command = "editor.quit", .chord = "Ctrl+Q" },
	DefaultShortcut{ .command = "editor.reload_layout", .chord = "F5" },
	DefaultShortcut{ .command = "editor.reload_styles", .chord = "F6" },
	DefaultShortcut{ .command = "devtools.ui_inspector", .chord = "Ctrl+Shift+I" },
	DefaultShortcut{ .command = "project.build", .chord = "Ctrl+B" },
	DefaultShortcut{ .command = "play.start", .chord = "Ctrl+P" },
	DefaultShortcut{ .command = "play.pause", .chord = "Ctrl+Shift+P" },
	DefaultShortcut{ .command = "play.stop", .chord = "Ctrl+Alt+P" },

	DefaultShortcut{ .command = "ui.zoom_in", .chord = "Ctrl+=" },
	DefaultShortcut{ .command = "ui.zoom_in", .chord = "Ctrl+Keypad+" },
	DefaultShortcut{ .command = "ui.zoom_out", .chord = "Ctrl+-" },
	DefaultShortcut{ .command = "ui.zoom_out", .chord = "Ctrl+Keypad-" },
	DefaultShortcut{ .command = "ui.zoom_reset", .chord = "Ctrl+0" },

	DefaultShortcut{ .command = "workspace.next", .chord = "Ctrl+PageDown" },
	DefaultShortcut{ .command = "workspace.previous", .chord = "Ctrl+PageUp" },

	DefaultShortcut{ .command = "entity.create", .chord = "Ctrl+Shift+N" },
	DefaultShortcut{ .command = "entity.delete", .chord = "Delete", .scope = "viewport" },
	DefaultShortcut{ .command = "entity.delete", .chord = "Delete", .scope = "hierarchy" },
	DefaultShortcut{ .command = "inspector.add_component", .chord = "Shift+S" },

	DefaultShortcut{ .command = "transform.translate", .chord = "W", .scope = "viewport" },
	DefaultShortcut{ .command = "transform.rotate", .chord = "E", .scope = "viewport" },
	DefaultShortcut{ .command = "transform.scale", .chord = "R", .scope = "viewport" },
	DefaultShortcut{ .command = "transform.axis_x", .chord = "X", .scope = "viewport" },
	DefaultShortcut{ .command = "transform.axis_y", .chord = "Y", .scope = "viewport" },
	DefaultShortcut{ .command = "transform.axis_z", .chord = "Z", .scope = "viewport" },
	DefaultShortcut{ .command = "transform.confirm", .chord = "Enter", .scope = "viewport" },
	DefaultShortcut{ .command = "transform.confirm", .chord = "KeypadEnter", .scope = "viewport" },
	DefaultShortcut{ .command = "transform.cancel", .chord = "Esc", .scope = "viewport" },
	DefaultShortcut{ .command = "transform.toggle_snapping", .chord = "Shift+Tab", .scope = "viewport" },
};

} // namespace

void bind_default_shortcuts(Aquila::UI::Core::Shortcuts &shortcuts) {
	for (const DefaultShortcut &shortcut : k_default_shortcuts) {
		shortcuts.bind(shortcut.command, shortcut.chord, shortcut.scope);
	}
}

void apply_user_shortcuts(Aquila::UI::Core::Shortcuts &shortcuts,
						  const std::map<std::string, std::vector<std::string>> &overrides) {
	using Aquila::Platform::Events::KeyChord;
	for (const auto &[command, texts] : overrides) {
		std::vector<KeyChord> chords;
		for (const std::string &text : texts) {
			if (const Option<KeyChord> chord = KeyChord::parse(text)) {
				chords.push_back(*chord);
			} else {
				AQUILA_LOG_WARNING("Editor preferences: '{}' for '{}' is not a valid shortcut", text, command);
			}
		}
		if (!texts.empty() && chords.empty()) {
			continue;
		}
		shortcuts.set_chords(command, std::move(chords));
	}
}

} // namespace Editor
