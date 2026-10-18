#include "UI/Inspectors/MeshComponentUI.h"

#include "Aquila/Graphics/Resources/MeshLibrary.h"
#include "Aquila/Graphics/Resources/ProceduralMeshes.h"
#include "Aquila/Scene/ComponentRegistry.h"
#include "Aquila/Scene/Components/MeshComponent.h"
#include "Aquila/UI/Core/TextureCache.h"
#include "Aquila/UI/Widgets/Dropdown.h"
#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/PropertyGrid.h"

namespace Editor {

using namespace Aquila;
using namespace Aquila::SceneManagement;
using namespace Aquila::SceneManagement::Components;
using Graphics::Resources::MeshGenerators;
using Graphics::Resources::MeshLibrary;
using Graphics::Resources::MeshSource;
using Graphics::Resources::MeshSourceKind;

namespace {

const char *primitive_icon(std::string_view id) {
	static constexpr std::array<std::pair<std::string_view, const char *>, 4> k_icons{ {
		{ "cube", "box" },
		{ "sphere", "circle" },
		{ "plane", "square" },
		{ "cylinder", "cylinder" },
	} };
	for (const auto &[generator, icon] : k_icons) {
		if (generator == id) {
			return icon;
		}
	}
	return "cuboid";
}

} // namespace

MeshComponentUI::MeshComponentUI(const ComponentDescriptor &descriptor, UI::Core::TextureCache *textures)
	: ReflectedComponentUI(descriptor, nullptr), m_textures(textures) {}

void MeshComponentUI::build(UI::Core::Collapsible *section, UI::Core::PropertyGrid *grid) {
	auto icon = [this](const char *name) -> GFX::GfxTexture * {
		return m_textures != nullptr ? m_textures->load("Engine/UI/Icons/" + std::string(name) + ".svg") : nullptr;
	};

	m_primitive = grid->add_row<UI::Core::Dropdown>("Primitive");
	m_primitive->set_icon(icon("cuboid"));
	for (const Graphics::Resources::MeshGenerator *generator : MeshGenerators::instance().all()) {
		m_primitive->add_option(generator->id, generator->title, icon(primitive_icon(generator->id)));
	}

	m_params_grid = grid->add_child<UI::Core::PropertyGrid>(grid->get_label_width());
	m_params_grid->set_split(grid->is_split());
	m_params_grid->set_hidden(true);

	ReflectedComponentUI::build(section, grid);

	m_vertices = grid->add_row<UI::Core::Label>("Vertices", std::string("0"));
	m_triangles = grid->add_row<UI::Core::Label>("Triangles", std::string("0"));
}

void MeshComponentUI::show(Entity entity) {
	ReflectedComponentUI::show(entity);
	m_entity = entity;
	m_params_generator.clear();
	sync(entity);

	m_primitive->on_changed.set(
		[this, entity](const std::string &generator) { set_source(entity, MeshSource::procedural(generator)); });
}

void MeshComponentUI::refresh() {
	ReflectedComponentUI::refresh();
	if (m_entity.is_valid() && m_entity.exists()) {
		sync(m_entity);
	}
}

void MeshComponentUI::on_reflected_edit(Entity entity) {
	sync(entity);
}

void MeshComponentUI::sync(Entity entity) {
	const auto &mesh = entity.get_component<MeshComponent>();
	const MeshSource source = mesh.is_valid() ? mesh.data->get_source() : MeshSource{};

	m_primitive->clear_selection();
	if (source.kind == MeshSourceKind::Procedural) {
		m_primitive->set_value(source.generator);
	} else {
		m_primitive->set_placeholder(source.kind == MeshSourceKind::Asset ? "Model"
										 : mesh.is_valid()				  ? "Custom"
																		  : "None");
	}

	show_params(source);
	update_stats(entity);
}

void MeshComponentUI::show_params(const MeshSource &source) {
	const bool procedural = source.kind == MeshSourceKind::Procedural;
	if (procedural && m_params_fields != nullptr && source.generator == m_params_generator) {
		if (m_params != source.params) {
			m_params = source.params;
			m_params_fields->refresh();
		}
		return;
	}

	m_params_fields.reset();
	while (!m_params_grid->get_children().empty()) {
		m_params_grid->remove_child(m_params_grid->get_children().back().get());
	}
	m_params_generator.clear();
	m_params.clear();

	const Reflection::TypeInfo *type = procedural ? MeshGenerators::instance().get_type(source.generator) : nullptr;
	m_params_grid->set_hidden(type == nullptr || type->get_properties().empty());
	if (type == nullptr) {
		return;
	}

	m_params_generator = source.generator;
	m_params = source.params;
	m_params_fields = std::make_unique<UI::Core::ReflectedPropertyGrid>(*m_params_grid, *type, nullptr);
	m_params_fields->bind(&m_params, [this] {
		if (m_entity.is_valid() && m_entity.exists()) {
			set_source(m_entity, MeshSource::procedural(m_params_generator, m_params));
		}
	});
}

void MeshComponentUI::set_source(Entity entity, const MeshSource &source) {
	Ref<Graphics::Resources::Mesh> mesh = MeshLibrary::instance().load(source);
	if (mesh == nullptr) {
		return;
	}
	entity.get_component<MeshComponent>().set_mesh(mesh);
	ReflectedComponentUI::refresh();
	sync(entity);
	notify_edited(entity);
}

void MeshComponentUI::update_stats(Entity entity) {
	const auto &mesh = entity.get_component<MeshComponent>();
	m_vertices->set_text(std::to_string(mesh.is_valid() ? mesh.data->get_vertex_count() : 0));
	m_triangles->set_text(std::to_string(mesh.is_valid() ? mesh.data->get_index_count() / 3 : 0));
}

void MeshComponentUI::notify_edited(Entity entity) {
	if (Signal<void()> *changed = get_descriptor().get_changed_signal(entity)) {
		(*changed)();
	}
	if (on_edited) {
		on_edited(entity);
	}
}

} // namespace Editor
