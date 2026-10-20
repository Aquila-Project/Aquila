#include "UI/Inspectors/MaterialComponentUI.h"

#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/Graphics/Material/MaterialAssets.h"
#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"
#include "Aquila/Scene/Components/MaterialComponent.h"
#include "Aquila/Scene/DefaultScenes.h"
#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/Collapsible.h"
#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/PropertyGrid.h"
#include "UI/Inspectors/MaterialEditorRows.h"

namespace Editor {

using namespace Aquila;
using namespace Aquila::SceneManagement;
using namespace Aquila::SceneManagement::Components;
using Platform::Filesystem::VirtualFileSystem;

namespace {

const std::string k_materials_dir = std::string(SharedConstants::PROJECT_CONTENT_DIR) + "/Materials";

std::string safe_file_name(std::string name) {
	for (char &c : name) {
		if (std::isalnum(static_cast<unsigned char>(c)) == 0 && c != '-' && c != '_') {
			c = '_';
		}
	}
	return name.empty() ? std::string("Material") : name;
}

} // namespace

MaterialComponentUI::MaterialComponentUI(const ComponentDescriptor &descriptor, GFX::GfxContext &context)
	: ReflectedComponentUI(descriptor, &context), m_context(context) {}

MaterialComponentUI::~MaterialComponentUI() = default;

void MaterialComponentUI::build(UI::Core::Collapsible *section, UI::Core::PropertyGrid *grid) {
	ReflectedComponentUI::build(section, grid);
	m_note = grid->add_row<UI::Core::Label>("Editing", std::string{});
	m_save_button = grid->add_row<UI::Core::Button>("", std::string("Save as Material Asset"));
	m_save_button->on_click.connect([this] {
		if (m_entity.is_valid() && m_entity.exists()) {
			save_as_asset(m_entity);
		}
	});
	m_rows = std::make_unique<MaterialEditorRows>(*grid, &m_context);
}

void MaterialComponentUI::show(Entity entity) {
	ReflectedComponentUI::show(entity);
	m_entity = entity;
	show_material(entity);
}

void MaterialComponentUI::refresh() {
	ReflectedComponentUI::refresh();
	if (m_rows != nullptr) {
		m_rows->refresh();
	}
}

void MaterialComponentUI::on_reflected_edit(Entity entity) {
	show_material(entity);
}

void MaterialComponentUI::show_material(Entity entity) {
	auto *component = entity.try_get_component<MaterialComponent>();
	if (component == nullptr) {
		m_rows->show(nullptr, {});
		return;
	}
	ensure_material(*component, m_context);

	const bool shared = component->uses_asset();
	m_note->set_text(shared ? std::filesystem::path(component->asset).filename().string() +
							 " (shared, edits change every object using it)"
							: std::string("This object's own material"));
	const bool has_project = VirtualFileSystem::get()->is_mounted(std::string(SharedConstants::PROJECT_MOUNT));
	m_save_button->get_parent()->set_hidden(shared || !has_project);

	m_rows->show(component->material, [this, entity](const Graphics::MaterialData &data, bool needs_rebuild) mutable {
		auto *target = entity.try_get_component<MaterialComponent>();
		if (target == nullptr) {
			return;
		}
		if (target->uses_asset()) {
			if (needs_rebuild) {
				target->material = Graphics::MaterialAssets::get()->update(m_context, target->asset, data);
			} else if (target->material) {
				Graphics::MaterialAssets::get()->save(*target->material);
			}
		} else {
			target->embedded = data;
			if (needs_rebuild) {
				target->material = Graphics::MaterialAssets::get()->create(m_context, data);
			}
		}
		if (needs_rebuild) {
			show_material(entity);
		}
		if (on_edited) {
			on_edited(entity);
		}
	});
}

void MaterialComponentUI::save_as_asset(Entity entity) {
	auto *component = entity.try_get_component<MaterialComponent>();
	if (component == nullptr || component->uses_asset()) {
		return;
	}
	auto *vfs = VirtualFileSystem::get();
	vfs->create_dir(k_materials_dir);

	const std::string stem = safe_file_name(entity.get_name());
	const std::string extension(Graphics::MATERIAL_EXTENSION);
	std::string path = k_materials_dir + "/" + stem + extension;
	for (int i = 1; vfs->exists(path); ++i) {
		path = k_materials_dir + "/" + stem + std::to_string(i) + extension;
	}

	const Graphics::MaterialData data = component->material ? component->material->get_data() : component->embedded;
	if (!Graphics::save_material_file(path, data)) {
		return;
	}
	component->asset = path;
	component->material = nullptr;
	ReflectedComponentUI::show(entity);
	show_material(entity);
	if (on_edited) {
		on_edited(entity);
	}
}

} // namespace Editor
