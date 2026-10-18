#include "UI/EditorWindows/HierarchyWindow.h"

#include "Aquila/Scene/Components/ScriptComponent.h"
#include "Aquila/Scene/EntityManager.h"
#include "Aquila/Scene/Scene.h"
#include "UI/ComponentIcons.h"
#include "UI/Panels/HierarchyPanel.h"
#include "UI/Panels/HierarchyTreeView.h"

namespace Editor {

using Aquila::SceneManagement::Entity;

HierarchyWindow::HierarchyWindow(EditorContext &context) : EditorWindow(context) {}

HierarchyWindow::~HierarchyWindow() = default;

void HierarchyWindow::build(Aquila::UI::Core::View &content) {
	if (auto layout = load_layout("hierarchy.aqlayout")) {
		content.add_child(std::move(layout));
	}

	m_panel = std::make_unique<HierarchyPanel>(*context().engine().get_scene().get_entity_manager());
	m_panel->build(&content, overlay_root());
	m_panel->set_tree_icons(context().icon("chevron-right"), context().icon("chevron-down"));
	m_panel->set_entity_icons(EntityIcons{
		.empty = context().icon(k_empty_entity_icon),
		.mesh = context().icon(component_icon("Mesh")),
		.light = context().icon(component_icon("Light")),
		.camera = context().icon(component_icon("Camera")),
		.sky = context().icon(component_icon("Sky Light")),
		.eye = context().icon("eye"),
		.eye_off = context().icon("eye-off"),
	});

	m_panel->on_create_requested.connect([this] {
		auto entity = context().engine().get_scene().get_entity_manager()->create_entity("New Entity");
		context().selection.on_entity_created(entity);
		context().selection.select(entity);
	});

	m_panel->on_delete_requested.connect([this](Entity entity) { context().destroy_entity(entity); });

	m_panel->on_entity_selected.connect([this](Entity entity) {
		if (!m_syncing) {
			context().selection.select(entity);
		}
	});
	m_panel->on_asset_dropped.connect([this](Entity entity, const Aquila::UI::Core::AssetPayload &asset) {
		if (asset.asset_type != "script") {
			return;
		}
		entity.get_or_emplace<Aquila::SceneManagement::Components::ScriptComponent>().path = asset.asset_path;
		context().selection.select(entity);
		context().selection.on_components_changed(entity);
	});
	m_panel->on_entity_deselected.connect([this] {
		if (!m_syncing) {
			context().selection.clear();
		}
	});

	EditorSelection &selection = context().selection;
	listen(selection.on_changed, [this](Entity entity) { show_selection(entity); });
	listen(selection.on_entity_created, [this](Entity entity) { m_panel->add_entity(entity); });
	listen(selection.on_entity_destroying, [this](Entity entity) { m_panel->remove_entity(entity); });
	listen(selection.on_entity_modified, [this](Entity entity) { m_panel->refresh_entity(entity); });
	listen(selection.on_scene_replaced, [this] { m_panel->rebuild(); });

	if (selection.has()) {
		show_selection(selection.get());
	}
}

void HierarchyWindow::show_selection(Entity entity) {
	m_syncing = true;
	if (entity.is_valid()) {
		m_panel->select_entity(entity);
	} else {
		m_panel->deselect_entity();
	}
	m_syncing = false;
}

} // namespace Editor
