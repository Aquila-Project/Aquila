#pragma once

#include "Aquila/Scene/SceneStatistics.h"

namespace Aquila::UI::Core {
class Label;
class View;
} // namespace Aquila::UI::Core

namespace Editor {

class StatusBar {
  public:
	void build(Aquila::UI::Core::View *layout_root);
	void update(const Aquila::SceneManagement::SceneStatistics &statistics, const std::string &selected_name);

	void set_visible(bool visible);
	[[nodiscard]] bool is_visible() const { return m_visible; }

  private:
	struct Field {
		Aquila::UI::Core::Label *label = nullptr;
		std::string text;
	};

	void show(Field &field, std::string text);

	Aquila::UI::Core::View *m_root = nullptr;
	Field m_selection;
	Field m_objects;
	Field m_lights;
	Field m_vertices;
	Field m_triangles;
	bool m_visible = true;
};

} // namespace Editor
