#include "UI/Components/TodoRow.h"

#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/Checkbox.h"
#include "Aquila/UI/Widgets/Label.h"

namespace Editor {

using namespace Aquila::UI::Core;

void TodoRow::define_behaviour() {
	m_done = get<Checkbox>("done");
	m_name = get<Label>("name");
	auto *remove = get<Button>("remove");
	if (m_done == nullptr || m_name == nullptr || remove == nullptr) {
		return;
	}
	listen(m_done->on_changed, [this](const bool &done) { on_done_changed(done); });
	listen(remove->on_click, [this] { on_remove(); });
}

void TodoRow::set_done(bool done) {
	if (m_done == nullptr) {
		return;
	}
	m_done->set_value_without_notify(done);
	m_name->set_class("todo-row-name-done", done);
}

}
