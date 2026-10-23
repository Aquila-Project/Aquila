#ifndef AQUILA_UI_COMMAND_H
#define AQUILA_UI_COMMAND_H

#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Aquila::UI::Core {
struct Command {
	std::string id;
	std::string title;
	Delegate<void()> run;
	Delegate<bool()> enabled;
	bool repeatable = false;
};

class Commands {
  public:
	void add(Command command);
	void remove(std::string_view id);

	[[nodiscard]] const Command *find(std::string_view id) const;
	[[nodiscard]] const std::vector<Command> &all() const { return m_commands; }

	[[nodiscard]] bool is_enabled(std::string_view id) const; // false when unknown
	/// @return false when the id is unknown or the command is disabled.
	bool execute(std::string_view id) const;

  private:
	std::vector<Command> m_commands;
};

}

#endif