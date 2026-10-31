#ifndef AQUILA_EDITOR_SETTINGS_SECTION_H
#define AQUILA_EDITOR_SETTINGS_SECTION_H

#include "Aquila/UI/Core/Behaviour.h"

namespace Aquila::UI::Core {
class Button;
class Label;
class PropertyGrid;
}

namespace Editor {

class SettingsSection final : public Aquila::UI::Core::Behaviour {
  public:
	void define_behaviour() override;
	void show(const std::string &title, const std::string &description, bool full, const std::string &reset_text);
	Aquila::UI::Core::View *add(std::string_view component, Aquila::UI::Core::ComponentArgs args = {});

	[[nodiscard]] Aquila::UI::Core::PropertyGrid *get_grid() const { return m_grid; }
	[[nodiscard]] Aquila::UI::Core::Button *get_reset() const { return m_reset; }

  private:
	Aquila::UI::Core::Label *m_title = nullptr;
	Aquila::UI::Core::Label *m_description = nullptr;
	Aquila::UI::Core::PropertyGrid *m_grid = nullptr;
	Aquila::UI::Core::Button *m_reset = nullptr;
};

}

#endif
