#ifndef AQUILA_EDITOR_BUILD_TEMPLATE_INFO_H
#define AQUILA_EDITOR_BUILD_TEMPLATE_INFO_H

#include "Aquila/UI/Core/Behaviour.h"

namespace Aquila::UI::Core {
class Label;
class SelectableTextView;
}

namespace Editor {

class BuildTemplateInfo final : public Aquila::UI::Core::Behaviour {
  public:
	void define_behaviour() override;
	void show(bool found, const std::string &directory);

  private:
	Aquila::UI::Core::Label *m_state = nullptr;
	Aquila::UI::Core::SelectableTextView *m_path = nullptr;
};

}

#endif
