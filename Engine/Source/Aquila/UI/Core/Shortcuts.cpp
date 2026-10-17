#include "Aquila/UI/Core/Shortcuts.h"

#include "Aquila/Foundation/Log.h"

namespace Aquila::UI::Core {

using Platform::Events::KeyChord;

void Shortcuts::add_command(Command command) {
	if (const auto it = std::ranges::find(m_commands, command.id, &Command::id); it != m_commands.end()) {
		AQUILA_LOG_WARNING("Shortcuts: command '{}' is registered twice, the last one wins", command.id);
		*it = std::move(command);
		return;
	}
	m_commands.push_back(std::move(command));
}

void Shortcuts::remove_command(std::string_view id) {
	std::erase_if(m_commands, [id](const Command &command) { return command.id == id; });
}

const Command *Shortcuts::find_command(std::string_view id) const {
	const auto it = std::ranges::find(m_commands, id, &Command::id);
	return it == m_commands.end() ? nullptr : &*it;
}

void Shortcuts::bind(std::string_view command, KeyChord chord, std::string_view scope) {
	for (const ShortcutBinding &existing : m_defaults) {
		if (existing.chord == chord && existing.scope == scope && existing.command != command) {
			AQUILA_LOG_WARNING("Shortcuts: {} is bound to both '{}' and '{}' in scope '{}'", chord.to_string(),
							   existing.command, command, scope.empty() ? "global" : scope);
		}
	}
	m_defaults.push_back({ .command = std::string(command), .chord = chord, .scope = std::string(scope) });
}

void Shortcuts::bind(std::string_view command, std::string_view chord, std::string_view scope) {
	if (const Option<KeyChord> parsed = KeyChord::parse(chord)) {
		bind(command, *parsed, scope);
		return;
	}
	AQUILA_LOG_ERROR("Shortcuts: '{}' for '{}' is not a valid key chord", chord, command);
}

void Shortcuts::set_chords(std::string_view command, std::vector<KeyChord> chords) {
	m_overrides[std::string(command)] = std::move(chords);
}

std::vector<ShortcutBinding> Shortcuts::effective_bindings() const {
	std::vector<ShortcutBinding> bindings;
	for (const ShortcutBinding &binding : m_defaults) {
		if (!m_overrides.contains(binding.command)) {
			bindings.push_back(binding);
		}
	}

	for (const auto &[command, chords] : m_overrides) {
		std::vector<std::string> scopes;
		for (const ShortcutBinding &binding : m_defaults) {
			if (binding.command == command && std::ranges::find(scopes, binding.scope) == scopes.end()) {
				scopes.push_back(binding.scope);
			}
		}
		if (scopes.empty()) {
			scopes.emplace_back();
		}
		for (const KeyChord &chord : chords) {
			for (const std::string &scope : scopes) {
				bindings.push_back({ .command = command, .chord = chord, .scope = scope });
			}
		}
	}
	return bindings;
}

std::vector<ShortcutBinding> Shortcuts::get_bindings(std::string_view command) const {
	std::vector<ShortcutBinding> bindings = effective_bindings();
	std::erase_if(bindings, [command](const ShortcutBinding &binding) { return binding.command != command; });
	return bindings;
}

std::string Shortcuts::get_label(std::string_view command) const {
	const std::vector<ShortcutBinding> bindings = get_bindings(command);
	return bindings.empty() ? std::string() : bindings.front().chord.to_string();
}

bool Shortcuts::is_scope_active(std::string_view scope) const {
	return scope.empty() || (m_scope_resolver && m_scope_resolver(scope));
}

bool Shortcuts::can_run(const Command &command) {
	return command.run && (!command.enabled || command.enabled());
}

bool Shortcuts::execute(std::string_view command_id) {
	const Command *command = find_command(command_id);
	if (command == nullptr || !can_run(*command)) {
		return false;
	}
	const Delegate<void()> run = command->run;
	run();
	return true;
}

bool Shortcuts::handle(const KeyChord &chord, bool is_repeat, bool is_typing) {
	if (!chord.is_valid() || chord.is_modifier_only() || (is_typing && !chord.works_while_typing())) {
		return false;
	}

	std::vector<ShortcutBinding> matches = effective_bindings();
	std::erase_if(matches, [&chord](const ShortcutBinding &binding) { return binding.chord != chord; });
	std::ranges::stable_partition(matches, [](const ShortcutBinding &binding) { return !binding.scope.empty(); });

	for (const ShortcutBinding &binding : matches) {
		if (!is_scope_active(binding.scope)) {
			continue;
		}
		const Command *command = find_command(binding.command);
		if (command == nullptr || !can_run(*command)) {
			continue;
		}
		if (is_repeat && !command->repeatable) {
			return true;
		}
		const Delegate<void()> run = command->run;
		run();
		return true;
	}
	return false;
}

}
