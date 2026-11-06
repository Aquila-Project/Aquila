#pragma once

#include "Aquila/Graphics/Resources/MeshSource.h"
#include "Aquila/Scene/Entity.h"
#include "Aquila/UI/Widgets/Inspectable.h"

namespace Aquila::UI::Core {
class Dropdown;
class Label;
class ReflectedPropertyGrid;
} // namespace Aquila::UI::Core

namespace Editor {

class EditorContext;

class MeshContent final : public Aquila::UI::Core::IInspectorContent {
  public:
	MeshContent(Aquila::SceneManagement::Entity entity, EditorContext &context);
	~MeshContent() override;

	void build_above(Aquila::UI::Core::PropertyGrid &grid) override;
	void build_below(Aquila::UI::Core::PropertyGrid &grid) override;
	void refresh() override;
	void on_property_edited() override;

  private:
	void sync();
	void show_params(const Aquila::Graphics::Resources::MeshSource &source);
	void set_source(const Aquila::Graphics::Resources::MeshSource &source);
	void update_stats();

	Aquila::SceneManagement::Entity m_entity;
	EditorContext &m_context;
	Aquila::UI::Core::Dropdown *m_primitive = nullptr;
	Aquila::UI::Core::PropertyGrid *m_params_grid = nullptr;
	Unique<Aquila::UI::Core::ReflectedPropertyGrid> m_params_fields;
	std::string m_params_generator;
	Aquila::Graphics::Resources::MeshParams m_params;
	Aquila::UI::Core::Label *m_vertices = nullptr;
	Aquila::UI::Core::Label *m_triangles = nullptr;
};

}
