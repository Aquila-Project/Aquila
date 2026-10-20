#include "Core/EditorContext.h"

#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/Scene/EntityManager.h"
#include "Aquila/Scene/Scene.h"
#include "Aquila/UI/Core/LayoutLoader.h"
#include "Aquila/UI/Core/TextureCache.h"
#include "Aquila/UI/Core/View.h"

namespace Editor {

using Aquila::SceneManagement::Entity;

void EditorSelection::select(Entity entity) {
	if (!entity.is_valid()) {
		clear();
		return;
	}
	if (m_entity == entity) {
		return;
	}
	m_asset.clear();
	m_entity = entity;
	on_changed(m_entity);
}

void EditorSelection::select_asset(const std::string &path) {
	clear();
	m_asset = path;
	on_asset_selected(m_asset);
}

void EditorSelection::clear() {
	if (m_entity.is_null()) {
		return;
	}
	m_entity = Entity::null();
	on_changed(m_entity);
}

bool EditorSelection::has() const {
	return m_entity.is_valid() && m_entity.exists();
}

EditorContext::EditorContext(Aquila::Application::EngineContext &engine, Aquila::UI::Core::Canvas &canvas,
							 Aquila::UI::Core::LayoutLoader &layouts, Aquila::UI::Core::TextureCache &textures,
							 std::string layout_dir)
	: m_engine(engine), m_canvas(canvas), m_layouts(layouts), m_textures(textures),
	  m_layout_dir(std::move(layout_dir)) {}

Aquila::GFX::GfxTexture *EditorContext::icon(std::string_view name) const {
	return m_textures.load("Engine/UI/Icons/" + std::string(name) + ".svg");
}

Unique<Aquila::UI::Core::View> EditorContext::load_layout(std::string_view file_name) const {
	return m_layouts.load_file(m_layout_dir + std::string(file_name));
}

void EditorContext::destroy_entity(Entity entity) {
	if (!entity.is_valid() || !entity.exists()) {
		return;
	}
	auto &entities = *m_engine.get_scene().get_entity_manager();
	const Entity selected = selection.get();
	if (selected.is_valid() && (selected == entity || entities.is_descendant(entity, selected))) {
		selection.clear();
	}
	selection.on_entity_destroying(entity);
	entities.destroy_entity(entity);
}

std::string EditorContext::to_native(const std::string &project_path) const {
	const std::string &mount = Aquila::SharedConstants::PROJECT_MOUNT;
	if (project_path != mount && !project_path.starts_with(mount + "/")) {
		return project_path;
	}
	const std::string relative = project_path.substr(std::min(project_path.size(), mount.size() + 1));
	return (std::filesystem::path(m_project.native_directory) / relative).lexically_normal().string();
}

Option<std::string> EditorContext::to_project_path(const std::string &native_path) const {
	namespace fs = std::filesystem;
	std::error_code error;
	const fs::path root = fs::weakly_canonical(m_project.native_directory, error);
	const fs::path file = fs::weakly_canonical(native_path, error);
	if (error) {
		return std::nullopt;
	}
	const fs::path relative = file.lexically_relative(root);
	if (relative.empty() || *relative.begin() == "..") {
		return std::nullopt;
	}
	if (relative == ".") {
		return Aquila::SharedConstants::PROJECT_MOUNT;
	}
	return Aquila::SharedConstants::PROJECT_MOUNT + "/" + relative.generic_string();
}

} // namespace Editor
