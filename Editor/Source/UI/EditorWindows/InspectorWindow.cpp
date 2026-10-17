#include "UI/EditorWindows/InspectorWindow.h"

#include "Aquila/Platform/Input.h"
#include "UI/Panels/InspectorPanel.h"

namespace Editor {

using Aquila::SceneManagement::Entity;

InspectorWindow::InspectorWindow(EditorContext &context) : EditorWindow(context) {}

InspectorWindow::~InspectorWindow() = default;

void InspectorWindow::build(Aquila::UI::Core::View &content) {
	if (auto layout = load_layout("inspector.aqlayout")) {
		content.add_child(std::move(layout));
	}

	m_panel = std::make_unique<InspectorPanel>(context().engine().get_context(), &context().textures());
	m_panel->build(&content, overlay_root());

	EditorSelection &selection = context().selection;
	m_panel->on_entity_renamed.connect([this](Entity entity) { context().selection.on_entity_modified(entity); });
	m_panel->on_components_changed.connect([this](Entity entity) { context().selection.on_entity_modified(entity); });

	listen(selection.on_changed, [this](Entity entity) { show(entity); });
	listen(selection.on_scene_replaced, [this] { m_panel->clear(); });
	listen(selection.on_entity_transformed, [this](Entity entity) { m_panel->refresh_values(entity); });
	listen(selection.on_components_changed, [this](Entity entity) {
		if (entity == context().selection.get()) {
			show(entity);
		}
	});

	add_command({
		.id = "inspector.add_component",
		.title = "Add component",
		.run = [this] { open_add_search(Aquila::Platform::Input::get_mouse_position()); },
		.enabled =
			[] {
				using Aquila::Platform::Input;
				using Aquila::Platform::Events::MouseButton;
				return !Input::is_mouse_button_pressed(MouseButton::Left) &&
					!Input::is_mouse_button_pressed(MouseButton::Right);
			},
	});

	if (selection.has()) {
		show(selection.get());
	}
}

void InspectorWindow::show(Entity entity) {
	if (entity.is_valid() && entity.exists()) {
		m_panel->show_entity(entity);
	} else {
		m_panel->clear();
	}
}

void InspectorWindow::open_add_search(Vec2 canvas_pos) {
	m_panel->open_add_search(canvas_pos);
}

} // namespace Editor
