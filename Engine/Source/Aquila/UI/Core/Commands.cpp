#include "Aquila/UI/Core/Command.h"

#include "Aquila/Foundation/Macros.h"

namespace Aquila::UI::Core {
bool Commands::execute(std::string_view id) const {
	const Command *cmd = find(id);
	if (cmd == nullptr || !is_enabled(id)) {
		return false;
	}

	const Delegate<void()> exec = cmd->run;
	exec();

	return true;
}

const Command *Commands::find(std::string_view id) const {
	if (auto it = std::ranges::find(m_commands, id, &Command::id); it != m_commands.end()) {
		return &*it;
	}

	return nullptr;
}

// keep it by value, we are using std::move to move ownership
void Commands::add(Command command) {
	if (const auto it = std::ranges::find(m_commands, command.id, &Command::id); it != m_commands.end()) {
		AQUILA_LOG_WARNING("Commands: '{}' is registered twice, the last one wins", command.id);
		*it = std::move(command);
		return;
	}
	m_commands.push_back(std::move(command));
}

bool Commands::is_enabled(std::string_view id) const {
	const Command *cmd = find(id);
	return cmd != nullptr && cmd->run && (!cmd->enabled || cmd->enabled());
}

void Commands::remove(std::string_view id) {
	if (auto it = std::ranges::find(m_commands, id, &Command::id); it != m_commands.end()) {
		m_commands.erase(it);
	}
}

}