#include "UI/EditorWindows/TodoWindow.h"

#include "Aquila/Foundation/FrameScheduler.h"
#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/Dropdown.h"
#include "Aquila/UI/Widgets/TextInput.h"
#include "UI/Components/TodoRow.h"

namespace Editor {

using namespace Aquila::UI::Core;

TodoWindow::TodoWindow(EditorContext &context) : EditorWindow(context, context.window_layout("todo")) {}

void TodoWindow::define_behaviour() {
	m_input = get<TextInput>("todo-input");
	m_filter = get<Dropdown>("todo-filter");
	m_list = get("todo-list");
	m_empty = get("todo-empty");
	auto *add = get<Button>("todo-add");
	if (m_input == nullptr || m_filter == nullptr || m_list == nullptr || m_empty == nullptr || add == nullptr) {
		return;
	}
	m_filter->set_value("all");
	listen(add->on_click, [this] { add_task(); });
	listen(m_input->on_submit, [this](const std::string &) { add_task(); });
	listen(m_filter->on_changed, [this](const std::string &) { refresh(); });
	rebuild();
}

void TodoWindow::update(F32 delta_time) {
	if (m_dirty) {
		m_dirty = false;
		rebuild();
	}
}

void TodoWindow::add_task() {
	if (m_input->get_text().empty()) {
		return;
	}
	m_tasks.push_back({ .text = m_input->get_text() });
	m_input->set_text("");
	refresh();
}

void TodoWindow::refresh() {
	m_dirty = true;
	Aquila::Foundation::FrameScheduler::get()->request_frame();
}

void TodoWindow::rebuild() {
	std::erase_if(m_tasks, [](const Task &task) { return task.removed; });
	m_list->clear_children();
	const std::string &filter = m_filter->get_value();
	for (Usize i = 0; i < m_tasks.size(); ++i) {
		const Task &task = m_tasks[i];
		if ((filter == "open" && task.done) || (filter == "done" && !task.done)) {
			continue;
		}
		auto *row = create<TodoRow>("TodoRow", *m_list, { .attributes = { { "label", task.text } } });
		if (row == nullptr) {
			continue;
		}
		row->set_done(task.done);
		row->on_done_changed.connect([this, i](const bool &done) {
			m_tasks[i].done = done;
			refresh();
		});
		row->on_remove.connect([this, i] {
			m_tasks[i].removed = true;
			refresh();
		});
	}
	m_empty->set_class("hidden", !m_list->get_children().empty());
}

}
