#include "UI/EditorWindows/InspectorWindow.h"

#include "Aquila/Graphics/Material/MaterialAssets.h"
#include "Aquila/Platform/Input.h"
#include "Aquila/UI/Widgets/Image.h"
#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/PropertyGrid.h"
#include "Aquila/UI/Widgets/ScrollView.h"
#include "Core/AssetThumbnails.h"
#include "UI/Inspectors/MaterialEditorRows.h"
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
	m_entity_root = content.find_by_id("inspector-root");
	build_asset_view(content);

	EditorSelection &selection = context().selection;
	m_panel->on_entity_renamed.connect([this](Entity entity) { context().selection.on_entity_modified(entity); });
	m_panel->on_components_changed.connect([this](Entity entity) { context().selection.on_entity_modified(entity); });

	listen(selection.on_changed, [this](Entity entity) { show(entity); });
	listen(selection.on_asset_selected, [this](const std::string &path) { show_asset(path); });
	listen(context().thumbnails().on_ready, [this](const std::string &path) {
		if (path == m_asset) {
			show_preview(path);
		}
	});
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

void InspectorWindow::build_asset_view(Aquila::UI::Core::View &content) {
	auto *root = content.add_child<Aquila::UI::Core::View>();
	root->add_class("asset-inspector-root");
	auto *scroll = root->add_child<Aquila::UI::Core::ScrollView>();
	scroll->add_class("asset-inspector-scroll");
	auto *body = scroll->add_child<Aquila::UI::Core::View>();
	body->add_class("asset-inspector");
	m_asset_preview = body->add_child<Aquila::UI::Core::Image>();
	m_asset_preview->add_class("asset-inspector-preview");
	m_asset_preview->set_hidden(true);
	m_asset_title = body->add_child<Aquila::UI::Core::Label>(std::string{});
	m_asset_title->add_class("asset-inspector-title");
	m_asset_path = body->add_child<Aquila::UI::Core::Label>(std::string{});
	m_asset_path->add_class("asset-inspector-path");
	auto *grid = body->add_child<Aquila::UI::Core::PropertyGrid>();
	grid->set_split(true);
	m_asset_rows = std::make_unique<MaterialEditorRows>(*grid, &context().engine().get_context());
	m_asset_view = root;
	m_asset_view->set_hidden(true);
}

void InspectorWindow::show_preview(const std::string &path) {
	Aquila::GFX::GfxTexture *texture = context().thumbnails().get(path);
	m_asset_preview->set_texture(texture);
	m_asset_preview->set_hidden(texture == nullptr);
}

void InspectorWindow::show_asset(const std::string &path) {
	if (!path.ends_with(Aquila::Graphics::MATERIAL_EXTENSION) || m_asset_rows == nullptr) {
		return;
	}
	m_asset = path;
	Aquila::GFX::GfxContext &gfx = context().engine().get_context();
	Ref<Aquila::Graphics::Material> material = Aquila::Graphics::MaterialAssets::get()->load(gfx, path);
	show_preview(path);
	m_asset_title->set_text(std::filesystem::path(path).stem().string());
	m_asset_path->set_text(path);
	m_asset_rows->show(material, [this, path](const Aquila::Graphics::MaterialData &data, bool needs_rebuild) {
		auto *assets = Aquila::Graphics::MaterialAssets::get();
		Aquila::GFX::GfxContext &ctx = context().engine().get_context();
		if (needs_rebuild) {
			assets->update(ctx, path, data);
			show_asset(path);
		} else if (Ref<Aquila::Graphics::Material> current = assets->load(ctx, path)) {
			assets->save(*current);
		}
	});
	if (m_entity_root != nullptr) {
		m_entity_root->set_hidden(true);
	}
	m_asset_view->set_hidden(false);
}

void InspectorWindow::show(Entity entity) {
	if (m_asset_view != nullptr && entity.is_valid()) {
		m_asset_view->set_hidden(true);
		m_asset.clear();
		if (m_entity_root != nullptr) {
			m_entity_root->set_hidden(false);
		}
	}
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
