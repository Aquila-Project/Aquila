#pragma once

#include "Aquila/Application/EngineContext.h"
#include "Aquila/Foundation/Signal.h"
#include "Aquila/Scene/Entity.h"
#include "Core/EditorTools.h"

#include <string>
#include <string_view>

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
}

namespace Editor {

class EditorSelection {
  public:
	void select(Aquila::SceneManagement::Entity entity);
	void clear();

	[[nodiscard]] Aquila::SceneManagement::Entity get() const { return m_entity; }
	[[nodiscard]] bool has() const;

	Signal<void(Aquila::SceneManagement::Entity)> on_changed;
	Signal<void(Aquila::SceneManagement::Entity)> on_entity_created;
	Signal<void(Aquila::SceneManagement::Entity)> on_entity_modified;
	Signal<void(Aquila::SceneManagement::Entity)> on_entity_transformed;
	Signal<void(Aquila::SceneManagement::Entity)> on_components_changed;
	Signal<void()> on_scene_replaced;

  private:
	Aquila::SceneManagement::Entity m_entity;
};

class EditorContext {
  public:
	EditorContext(Aquila::Application::EngineContext &engine, Aquila::UI::Core::Canvas &canvas,
				  Aquila::UI::Core::LayoutLoader &layouts, Aquila::UI::Core::TextureCache &textures, std::string layout_dir);

	[[nodiscard]] Aquila::Application::EngineContext &engine() const { return m_engine; }
	[[nodiscard]] Aquila::UI::Core::Canvas &canvas() const { return m_canvas; }
	[[nodiscard]] Aquila::UI::Core::LayoutLoader &layouts() const { return m_layouts; }
	[[nodiscard]] Aquila::UI::Core::TextureCache &textures() const { return m_textures; }

	[[nodiscard]] Aquila::UI::Core::View *overlay_root() const { return m_overlay_root; }
	void set_overlay_root(Aquila::UI::Core::View *root) { m_overlay_root = root; }

	[[nodiscard]] Aquila::Rendering::CameraController *camera() const { return m_camera; }
	void set_camera(Aquila::Rendering::CameraController *camera) { m_camera = camera; }

	[[nodiscard]] Aquila::GFX::GfxTexture *icon(std::string_view name) const;
	[[nodiscard]] Unique<Aquila::UI::Core::View> load_layout(std::string_view file_name) const;

	EditorSelection selection;
	EditorTools tools;
	Signal<void()> on_render_output_changed;

  private:
	Aquila::Application::EngineContext &m_engine;
	Aquila::UI::Core::Canvas &m_canvas;
	Aquila::UI::Core::LayoutLoader &m_layouts;
	Aquila::UI::Core::TextureCache &m_textures;
	std::string m_layout_dir;
	Aquila::UI::Core::View *m_overlay_root = nullptr;
	Aquila::Rendering::CameraController *m_camera = nullptr;
};

}
