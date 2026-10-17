#ifndef AQUILA_UI_CORE_SHORTCUTS_H
#define AQUILA_UI_CORE_SHORTCUTS_H

#include "Aquila/Foundation/Defines.h"
#include "Aquila/Platform/Events/KeyChord.h"

namespace Aquila::UI::Core {

struct Command {
	std::string id;
	std::string title;
	Delegate<void()> run;
	Delegate<bool()> enabled{};
	bool repeatable = false;
};

struct ShortcutBinding {
	std::string command;
	Platform::Events::KeyChord chord;
	std::string scope{};
};

/// Commands plus the keymap. Scoped bindings win over global ones,
/// and a disabled command lets the key fall through.
///
/// @code
/// shortcuts.add_command({ .id = "scene.save", .title = "Save Scene", .run = [this] { save(); } });
/// shortcuts.bind("scene.save", "Ctrl+S");
/// shortcuts.bind("entity.delete", "Delete", "hierarchy");
/// @endcode
class Shortcuts {
  public:
	using ScopeResolver = Delegate<bool(std::string_view scope)>;

	void add_command(Command command);
	void remove_command(std::string_view id);
	[[nodiscard]] const Command *find_command(std::string_view id) const;
	[[nodiscard]] const std::vector<Command> &get_commands() const { return m_commands; }

	void bind(std::string_view command, Platform::Events::KeyChord chord, std::string_view scope = {});
	void bind(std::string_view command, std::string_view chord, std::string_view scope = {});
	/// User overrides, they keep the scope of the default binding. An empty list unbinds the command.
	void set_chords(std::string_view command, std::vector<Platform::Events::KeyChord> chords);
	/// Drops the user override, the default bindings come back.
	void reset_chords(std::string_view command) { m_overrides.erase(std::string(command)); }
	[[nodiscard]] bool has_override(std::string_view command) const {
		return m_overrides.contains(std::string(command));
	}
	[[nodiscard]] std::vector<ShortcutBinding> get_bindings(std::string_view command) const;
	/// The first bound chord as text, like "Ctrl+S", for menus. Empty when unbound.
	[[nodiscard]] std::string get_label(std::string_view command) const;

	/// Decides whether a scope is active right now. In the editor: is the mouse over that window.
	void set_scope_resolver(ScopeResolver resolver) { m_scope_resolver = std::move(resolver); }

	bool execute(std::string_view command);
	/// @return false when nothing ran, so the key can go somewhere else.
	/// Key repeats only run repeatable commands, the others swallow the repeat.
	bool handle(const Platform::Events::KeyChord &chord, bool is_repeat, bool is_typing);

  private:
	[[nodiscard]] std::vector<ShortcutBinding> effective_bindings() const;
	[[nodiscard]] bool is_scope_active(std::string_view scope) const;
	[[nodiscard]] static bool can_run(const Command &command);

	std::vector<Command> m_commands;
	std::vector<ShortcutBinding> m_defaults;
	std::unordered_map<std::string, std::vector<Platform::Events::KeyChord>> m_overrides;
	ScopeResolver m_scope_resolver;
};

} // namespace Aquila::UI::Core

#endif
