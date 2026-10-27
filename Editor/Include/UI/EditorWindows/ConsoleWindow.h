#pragma once

#include "Core/EditorWindow.h"

namespace Editor {

class ConsolePanel;

class ConsoleWindow final : public EditorWindow {
  public:
	explicit ConsoleWindow(EditorContext &context);
	~ConsoleWindow() override;

	void define_behaviour() override;
	void update(F32 delta_time) override;
	void clear();

  private:
	Unique<ConsolePanel> m_panel;
};

}
