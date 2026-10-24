#include "UI/Inspector/MaterialContent.h"

#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/Foundation/Text/StringUtils.h"
#include "Aquila/Graphics/Material/MaterialAssets.h"
#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"
#include "Aquila/Scene/Components/MaterialComponent.h"
#include "Aquila/Scene/DefaultScenes.h"
#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/PropertyGrid.h"
#include "Core/EditorContext.h"
#include "UI/Inspector/MaterialEditorRows.h"

namespace Editor {

using Aquila::Graphics::MaterialAssets;
using Aquila::Platform::Filesystem::VirtualFileSystem;
using Aquila::SceneManagement::Entity;
using Aquila::SceneManagement::Components::MaterialComponent;

namespace {

const std::string K_MATERIALS_DIR = std::string(Aquila::SharedConstants::PROJECT_CONTENT_DIR) + "/Materials";

}

MaterialContent::MaterialContent(Entity entity, EditorContext &context) : m_entity(entity), m_context(context) {}

MaterialContent::~MaterialContent() = default;

void MaterialContent::build_below(Aquila::UI::Core::PropertyGrid &grid) {
	m_note = grid.add_row<Aquila::UI::Core::Label>("Editing", std::string{});
	m_save_button = grid.add_row<Aquila::UI::Core::Button>("", std::string("Save as Material Asset"));
	m_save_button->on_click.connect([this] { save_as_asset(); });
	m_rows = std::make_unique<MaterialEditorRows>(grid, &m_context.engine().get_context());
	show_material();
}

void MaterialContent::refresh() {
	if (m_rows != nullptr) {
		m_rows->refresh();
	}
}

void MaterialContent::on_property_edited() {
	show_material();
}

void MaterialContent::show_material() {
	auto *component = m_entity.exists() ? m_entity.try_get_component<MaterialComponent>() : nullptr;
	if (component == nullptr || m_rows == nullptr) {
		if (m_rows != nullptr) {
			m_rows->show(nullptr, {});
		}
		return;
	}
	Aquila::GFX::GfxContext &gfx = m_context.engine().get_context();
	Aquila::SceneManagement::ensure_material(*component, gfx);

	const bool shared = component->uses_asset();
	m_note->set_text(shared ? std::filesystem::path(component->asset).filename().string() +
							 " (shared, edits change every object using it)"
							: std::string("This object's own material"));
	const bool has_project = VirtualFileSystem::get()->is_mounted(std::string(Aquila::SharedConstants::PROJECT_MOUNT));
	m_save_button->get_parent()->set_hidden(shared || !has_project);

	m_rows->show(component->material, [this, &gfx](const Aquila::Graphics::MaterialData &data, bool needs_rebuild) {
		auto *target = m_entity.exists() ? m_entity.try_get_component<MaterialComponent>() : nullptr;
		if (target == nullptr) {
			return;
		}
		if (target->uses_asset()) {
			target->material = MaterialAssets::get()->edit(gfx, target->asset, data, needs_rebuild);
		} else {
			target->embedded = data;
			if (needs_rebuild) {
				target->material = MaterialAssets::get()->create(gfx, data);
			}
		}
		if (needs_rebuild) {
			show_material();
		}
		if (notify_edited) {
			notify_edited();
		}
	});
}

void MaterialContent::save_as_asset() {
	auto *component = m_entity.exists() ? m_entity.try_get_component<MaterialComponent>() : nullptr;
	if (component == nullptr || component->uses_asset()) {
		return;
	}
	auto *vfs = VirtualFileSystem::get();
	vfs->create_dir(K_MATERIALS_DIR);

	std::string stem = Aquila::Foundation::to_file_name(m_entity.get_name());
	if (stem.empty()) {
		stem = "Material";
	}
	const std::string path =
		vfs->unique_path(K_MATERIALS_DIR, stem + std::string(Aquila::Graphics::MATERIAL_EXTENSION));

	const Aquila::Graphics::MaterialData data =
		component->material ? component->material->get_data() : component->embedded;
	if (!Aquila::Graphics::save_material_file(path, data)) {
		return;
	}
	component->asset = path;
	component->material = nullptr;
	show_material();
	if (notify_edited) {
		notify_edited();
	}
}

}
