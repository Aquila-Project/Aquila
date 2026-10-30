#include "UI/EditorWindows/HierarchyWindow.h"

#include "Aquila/Scene/Components/ScriptComponent.h"
#include "Aquila/Scene/EntityManager.h"
#include "Aquila/Scene/Scene.h"
#include "UI/Panels/HierarchyPanel.h"
#include "UI/Panels/HierarchyTreeView.h"

namespace Editor {

using Aquila::SceneManagement::Entity;

HierarchyWindow::HierarchyWindow(EditorContext &context) : EditorWindow(context, context.window_layout("hierarchy")) {}

HierarchyWindow::~HierarchyWindow() = default;

void HierarchyWindow::define_behaviour() {
	m_panel = std::make_unique<HierarchyPanel>(*context().engine().get_scene().get_entity_manager(),
											   context().window_layout("hierarchy"));
	m_panel->build(&root(), overlay_root());
	m_panel->set_tree_icons(context().icon("chevron-right"), context().icon("chevron-down"));
	m_panel->set_entity_icons(EntityIcons{
		.entity = [this](Entity entity) { return context().icon(context().component_editors.entity_icon(entity)); },
		.eye = context().icon("eye"),
		.eye_off = context().icon("eye-off"),
	});

	m_panel->on_create_requested.connect([this](Entity parent) { context().create_entity(parent); });

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
	listen(context().engine().get_scene().on_entities_changed, [this] { m_scene_changed = true; });

	if (selection.has()) {
		show_selection(selection.get());
	}
}

void HierarchyWindow::update(F32) {
	if (!m_scene_changed) {
		return;
	}
	m_scene_changed = false;
	m_panel->sync();
	if (context().selection.has()) {
		show_selection(context().selection.get());
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
