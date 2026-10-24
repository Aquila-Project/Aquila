#include "UI/Inspector/MeshContent.h"

#include "Aquila/Graphics/Resources/MeshLibrary.h"
#include "Aquila/Graphics/Resources/ProceduralMeshes.h"
#include "Aquila/Scene/Components/MeshComponent.h"
#include "Aquila/UI/Widgets/Dropdown.h"
#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/PropertyGrid.h"
#include "Aquila/UI/Widgets/ReflectedPropertyGrid.h"
#include "Core/EditorContext.h"

namespace Editor {

using Aquila::Graphics::Resources::MeshGenerators;
using Aquila::Graphics::Resources::MeshLibrary;
using Aquila::Graphics::Resources::MeshSource;
using Aquila::Graphics::Resources::MeshSourceKind;
using Aquila::SceneManagement::Entity;
using Aquila::SceneManagement::Components::MeshComponent;

namespace {

std::string_view primitive_icon(std::string_view id) {
	static constexpr std::array<std::pair<std::string_view, std::string_view>, 4> K_ICONS{ {
		{ "cube", "box" },
		{ "sphere", "circle" },
		{ "plane", "square" },
		{ "cylinder", "cylinder" },
	} };
	for (const auto &[generator, icon] : K_ICONS) {
		if (generator == id) {
			return icon;
		}
	}
	return "cuboid";
}

}

MeshContent::MeshContent(Entity entity, EditorContext &context) : m_entity(entity), m_context(context) {}

MeshContent::~MeshContent() = default;

void MeshContent::build_above(Aquila::UI::Core::PropertyGrid &grid) {
	m_primitive = grid.add_row<Aquila::UI::Core::Dropdown>("Primitive");
	m_primitive->set_icon(m_context.icon("cuboid"));
	for (const Aquila::Graphics::Resources::MeshGenerator *generator : MeshGenerators::instance().all()) {
		m_primitive->add_option(generator->id, generator->title, m_context.icon(primitive_icon(generator->id)));
	}
	m_primitive->on_changed.set(
		[this](const std::string &generator) { set_source(MeshSource::procedural(generator)); });

	m_params_grid = grid.add_child<Aquila::UI::Core::PropertyGrid>(grid.get_label_width());
	m_params_grid->set_split(grid.is_split());
	m_params_grid->set_hidden(true);
}

void MeshContent::build_below(Aquila::UI::Core::PropertyGrid &grid) {
	m_vertices = grid.add_row<Aquila::UI::Core::Label>("Vertices", std::string("0"));
	m_triangles = grid.add_row<Aquila::UI::Core::Label>("Triangles", std::string("0"));
	sync();
}

void MeshContent::refresh() {
	sync();
}

void MeshContent::on_property_edited() {
	sync();
}

void MeshContent::sync() {
	const auto *mesh = m_entity.exists() ? m_entity.try_get_component<MeshComponent>() : nullptr;
	if (mesh == nullptr || m_primitive == nullptr) {
		return;
	}
	const MeshSource source = mesh->is_valid() ? mesh->data->get_source() : MeshSource{};

	m_primitive->clear_selection();
	if (source.kind == MeshSourceKind::Procedural) {
		m_primitive->set_value(source.generator);
	} else {
		m_primitive->set_placeholder(source.kind == MeshSourceKind::Asset ? "Model"
										 : mesh->is_valid()				  ? "Custom"
																		  : "None");
	}

	show_params(source);
	update_stats();
}

void MeshContent::show_params(const MeshSource &source) {
	const bool procedural = source.kind == MeshSourceKind::Procedural;
	if (procedural && m_params_fields != nullptr && source.generator == m_params_generator) {
		if (m_params != source.params) {
			m_params = source.params;
			m_params_fields->refresh();
		}
		return;
	}

	m_params_fields.reset();
	m_params_grid->clear_children();
	m_params_generator.clear();
	m_params.clear();

	const Aquila::Reflection::TypeInfo *type =
		procedural ? MeshGenerators::instance().get_type(source.generator) : nullptr;
	m_params_grid->set_hidden(type == nullptr || type->get_properties().empty());
	if (type == nullptr) {
		return;
	}

	m_params_generator = source.generator;
	m_params = source.params;
	m_params_fields = std::make_unique<Aquila::UI::Core::ReflectedPropertyGrid>(*m_params_grid, *type, nullptr);
	m_params_fields->bind(&m_params, [this] { set_source(MeshSource::procedural(m_params_generator, m_params)); });
}

void MeshContent::set_source(const MeshSource &source) {
	auto *mesh = m_entity.exists() ? m_entity.try_get_component<MeshComponent>() : nullptr;
	if (mesh == nullptr) {
		return;
	}
	Ref<Aquila::Graphics::Resources::Mesh> loaded = MeshLibrary::instance().load(source);
	if (loaded == nullptr) {
		return;
	}
	mesh->set_mesh(loaded);
	sync();
	if (notify_edited) {
		notify_edited();
	}
}

void MeshContent::update_stats() {
	const auto *mesh = m_entity.exists() ? m_entity.try_get_component<MeshComponent>() : nullptr;
	if (mesh == nullptr || m_vertices == nullptr || m_triangles == nullptr) {
		return;
	}
	m_vertices->set_text(std::to_string(mesh->is_valid() ? mesh->data->get_vertex_count() : 0));
	m_triangles->set_text(std::to_string(mesh->is_valid() ? mesh->data->get_index_count() / 3 : 0));
}

}
