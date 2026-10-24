#include "UI/EditorWindows/InspectorWindow.h"

#include "Aquila/Platform/Input.h"
#include "Aquila/UI/Widgets/InspectorView.h"
#include "UI/Inspector/EntityInspector.h"

namespace Editor {

using Aquila::SceneManagement::Entity;

InspectorWindow::InspectorWindow(EditorContext &context) : EditorWindow(context) {}

InspectorWindow::~InspectorWindow() = default;

void InspectorWindow::build(Aquila::UI::Core::View &content) {
	if (auto layout = load_layout("inspector.aqlayout")) {
		content.add_child(std::move(layout));
	}

	m_entities = std::make_unique<EntityInspector>(context());
	if (Aquila::UI::Core::View *root = content.find_by_id("inspector-root")) {
		m_view = std::make_unique<Aquila::UI::Core::InspectorView>(
			*root, [this](std::string_view name) { return context().icon(name); }, &context().engine().get_context());
		m_view->select_tab("Components");
	}

	EditorSelection &selection = context().selection;
	listen(selection.on_changed, [this](Entity entity) { show_entity(entity); });
	listen(selection.on_asset_selected, [this](const std::string &path) { show_asset(path); });
	listen(selection.on_scene_replaced, [this] {
		if (m_view != nullptr && m_asset.empty()) {
			m_view->clear();
		}
	});
	listen(selection.on_entity_transformed, [this](Entity entity) {
		if (m_view != nullptr && m_asset.empty() && entity == context().selection.get()) {
			m_view->refresh();
		}
	});
	listen(selection.on_components_changed, [this](Entity entity) {
		if (entity == context().selection.get()) {
			show_entity(entity);
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
		show_entity(selection.get());
	} else if (!selection.get_asset().empty()) {
		show_asset(selection.get_asset());
	}
}

void InspectorWindow::update(F32 delta_time) {
	if (m_view != nullptr) {
		m_view->update();
	}
}

void InspectorWindow::open_add_search(Vec2 canvas_pos) {
	if (m_asset.empty()) {
		m_entities->open_add_menu(canvas_pos, true);
	}
}

void InspectorWindow::show_entity(Entity entity) {
	if (m_view == nullptr) {
		return;
	}
	if (entity.is_valid() && entity.exists()) {
		m_asset.clear();
		m_view->show(m_entities->inspect(entity));
	} else if (m_asset.empty()) {
		m_view->clear();
	}
}

void InspectorWindow::show_asset(const std::string &path) {
	const AssetType *type = context().asset_types.find_for_path(path);
	if (m_view == nullptr || type == nullptr || !type->inspect) {
		return;
	}
	if (Option<Aquila::UI::Core::Inspectable> inspectable = type->inspect(path, context())) {
		m_asset = path;
		m_view->show(std::move(*inspectable));
	}
}
}
