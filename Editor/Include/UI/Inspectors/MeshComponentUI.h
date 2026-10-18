#pragma once

#include "UI/Inspectors/ReflectedComponentUI.h"

#include "Aquila/Graphics/Resources/MeshSource.h"

namespace Aquila::UI::Core {
class Dropdown;
class Label;
class TextureCache;
} // namespace Aquila::UI::Core

namespace Editor {

/// Mesh section: primitive picker with the generator's parameters, the reflected Model slot, then stats.
class MeshComponentUI : public ReflectedComponentUI {
  public:
	MeshComponentUI(const Aquila::SceneManagement::ComponentDescriptor &descriptor,
					Aquila::UI::Core::TextureCache *textures);

	void build(Aquila::UI::Core::Collapsible *section, Aquila::UI::Core::PropertyGrid *grid) override;
	void show(Aquila::SceneManagement::Entity entity) override;
	void refresh() override;

  protected:
	void on_reflected_edit(Aquila::SceneManagement::Entity entity) override;

  private:
	void sync(Aquila::SceneManagement::Entity entity);
	void show_params(const Aquila::Graphics::Resources::MeshSource &source);
	void set_source(Aquila::SceneManagement::Entity entity, const Aquila::Graphics::Resources::MeshSource &source);
	void update_stats(Aquila::SceneManagement::Entity entity);
	void notify_edited(Aquila::SceneManagement::Entity entity);

	Aquila::UI::Core::TextureCache *m_textures = nullptr;
	Aquila::UI::Core::Dropdown *m_primitive = nullptr;
	Aquila::UI::Core::PropertyGrid *m_params_grid = nullptr;
	Unique<Aquila::UI::Core::ReflectedPropertyGrid> m_params_fields;
	std::string m_params_generator;
	Aquila::Graphics::Resources::MeshParams m_params;
	Aquila::UI::Core::Label *m_vertices = nullptr;
	Aquila::UI::Core::Label *m_triangles = nullptr;
	Aquila::SceneManagement::Entity m_entity;
};

} // namespace Editor
