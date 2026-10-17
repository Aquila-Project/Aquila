#ifndef AQUILA_EDITOR_DEFAULT_SHORTCUTS_H
#define AQUILA_EDITOR_DEFAULT_SHORTCUTS_H

namespace Aquila::UI::Core {
class Shortcuts;
}

namespace Editor {

void bind_default_shortcuts(Aquila::UI::Core::Shortcuts &shortcuts);
/// From the preferences JSON, `"shortcuts": { "entity.delete": ["Delete", "X"] }`. An empty list unbinds.
void apply_user_shortcuts(Aquila::UI::Core::Shortcuts &shortcuts,
						  const std::map<std::string, std::vector<std::string>> &overrides);

}

#endif
