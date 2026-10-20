#pragma once

#include "Aquila/Application/EngineContext.h"
#include "Aquila/Foundation/Math/Rect.h"
#include "Aquila/Foundation/Signal.h"
#include "Aquila/Scene/Entity.h"
#include "Aquila/UI/Core/Shortcuts.h"
#include "Core/ContentCreators.h"
#include "Core/EditorTools.h"
#include "Core/ProjectManager.h"

namespace Aquila::GFX {
class GfxTexture;
}

namespace Aquila::Rendering {
class CameraController;
}

namespace Aquila::UI::Core {
class Canvas;
class LayoutLoader;
class TextureCache;
class View;
} // namespace Aquila::UI::Core

namespace Editor {

class AssetThumbnails;

/// The selection, and the signals windows use to keep each other in sync.
class EditorSelection {
  public:
	void select(Aquila::SceneManagement::Entity entity);
	void clear();

	[[nodiscard]] Aquila::SceneManagement::Entity get() const { return m_entity; }
	[[nodiscard]] bool has() const;

	/// A content browser file the inspector shows instead of an entity (a .aqmat). Clears the entity selection,
	/// selecting an entity clears it.
	void select_asset(const std::string &path);
	[[nodiscard]] const std::string &get_asset() const { return m_asset; }

	Signal<void(Aquila::SceneManagement::Entity)> on_changed;
	Signal<void(Aquila::SceneManagement::Entity)> on_entity_created;
	Signal<void(Aquila::SceneManagement::Entity)> on_entity_destroying;
	/// Once an edit is done: gizmo released, inspector field changed.
	Signal<void(Aquila::SceneManagement::Entity)> on_entity_modified;
	/// Over and over while the gizmo drags, so the inspector can follow.
	Signal<void(Aquila::SceneManagement::Entity)> on_entity_transformed;
	Signal<void(Aquila::SceneManagement::Entity)> on_components_changed;
	/// A different scene was loaded, drop anything that points into the old one.
	Signal<void()> on_scene_replaced;
	Signal<void(const std::string &)> on_asset_selected;

  private:
	Aquila::SceneManagement::Entity m_entity;
	std::string m_asset;
};

/// What every editor window can reach.
class EditorContext {
  public:
	EditorContext(Aquila::Application::EngineContext &engine, Aquila::UI::Core::Canvas &canvas,
				  Aquila::UI::Core::LayoutLoader &layouts, Aquila::UI::Core::TextureCache &textures,
				  std::string layout_dir);

	[[nodiscard]] Aquila::Application::EngineContext &engine() const { return m_engine; }
	[[nodiscard]] Aquila::UI::Core::Canvas &canvas() const { return m_canvas; }
	[[nodiscard]] Aquila::UI::Core::LayoutLoader &layouts() const { return m_layouts; }
	[[nodiscard]] Aquila::UI::Core::TextureCache &textures() const { return m_textures; }
	/// Images and rendered materials for content files. Set by EditorModule right after the context is made.
	[[nodiscard]] AssetThumbnails &thumbnails() const { return *m_thumbnails; }
	void set_thumbnails(AssetThumbnails *thumbnails) { m_thumbnails = thumbnails; }

	[[nodiscard]] Aquila::UI::Core::View *overlay_root() const { return m_overlay_root; }
	void set_overlay_root(Aquila::UI::Core::View *root) { m_overlay_root = root; }

	[[nodiscard]] const ProjectInfo &project() const { return m_project; }
	void set_project(ProjectInfo project) { m_project = std::move(project); }
	/// Changes the open project's settings. EditorModule saves them and fires on_settings_changed.
	void set_project_settings(ProjectSettings settings) {
		if (m_project_settings_writer) {
			m_project_settings_writer(std::move(settings));
		}
	}
	void set_project_settings_writer(Delegate<void(ProjectSettings)> writer) {
		m_project_settings_writer = std::move(writer);
	}
	/// "/project/Content/a.png" to the file on disk. Other VFS paths come back unchanged.
	[[nodiscard]] std::string to_native(const std::string &project_path) const;
	/// The other way, nullopt for files outside the open project (scenes can only reference files inside it).
	[[nodiscard]] Option<std::string> to_project_path(const std::string &native_path) const;

	[[nodiscard]] Aquila::Rendering::CameraController *camera() const { return m_camera; }
	void set_camera(Aquila::Rendering::CameraController *camera) { m_camera = camera; }

	/// See EditorWindows::is_visible(). False until the editor module hands over the window registry.
	[[nodiscard]] bool is_window_visible(std::string_view type_id) const {
		return m_window_visible && m_window_visible(type_id);
	}
	void set_window_visibility(Delegate<bool(std::string_view)> resolver) { m_window_visible = std::move(resolver); }

	/// Where the Game view shows the game while it's live, in main window pixels. GameWindow keeps it current.
	[[nodiscard]] Option<Rect> get_game_view() const { return m_game_view; }
	void set_game_view(Option<Rect> rect) { m_game_view = rect; }
	/// The game gets the keyboard and mouse: the Game view was clicked last. EditorModule keeps it.
	[[nodiscard]] bool is_game_focused() const { return m_game_focused; }
	void set_game_focused(bool focused) { m_game_focused = focused; }

	/// A Lucide icon by name, like "list-tree" (Resources/Engine/UI/Icons/list-tree.svg).
	[[nodiscard]] Aquila::GFX::GfxTexture *icon(std::string_view name) const;
	/// A file from the editor's layout folder, by name.
	[[nodiscard]] Unique<Aquila::UI::Core::View> load_layout(std::string_view file_name) const;

	/// Clears the selection when it or one of its parents is selected, fires on_entity_destroying,
	/// then destroys it with its children. Always delete entities through this.
	void destroy_entity(Aquila::SceneManagement::Entity entity);

	EditorSelection selection;
	EditorTools tools;
	Aquila::UI::Core::Shortcuts shortcuts;
	std::vector<ContentCreator> content_creators;
	Signal<void()> on_render_output_changed;
	/// Fires after the preferences (Config::get_preferences()) or the project settings changed, from the Settings
	/// window. Windows that read either apply them again.
	Signal<void()> on_settings_changed;

  private:
	Aquila::Application::EngineContext &m_engine;
	Aquila::UI::Core::Canvas &m_canvas;
	Aquila::UI::Core::LayoutLoader &m_layouts;
	Aquila::UI::Core::TextureCache &m_textures;
	AssetThumbnails *m_thumbnails = nullptr;
	std::string m_layout_dir;
	Aquila::UI::Core::View *m_overlay_root = nullptr;
	Aquila::Rendering::CameraController *m_camera = nullptr;
	Delegate<bool(std::string_view)> m_window_visible;
	Option<Rect> m_game_view;
	bool m_game_focused = false;
	ProjectInfo m_project;
	Delegate<void(ProjectSettings)> m_project_settings_writer;
};

} // namespace Editor
