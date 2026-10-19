#include "UI/Panels/InspectorPanel.h"

#include "Aquila/Foundation/Macros.h"
#include "Aquila/Scene/ComponentRegistry.h"
#include "Aquila/Scene/Components/CameraComponent.h"
#include "Aquila/Scene/Components/LightComponent.h"
#include "Aquila/Scene/Components/MaterialComponent.h"
#include "Aquila/Scene/Components/MeshComponent.h"
#include "Aquila/Scene/Components/SkyLightComponent.h"
#include "Aquila/Scene/DefaultScenes.h"
#include "Aquila/UI/Core/TextureCache.h"
#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/Collapsible.h"
#include "Aquila/UI/Widgets/Dropdown.h"
#include "Aquila/UI/Widgets/Image.h"
#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/PopupMenu.h"
#include "Aquila/UI/Widgets/PropertyGrid.h"
#include "Aquila/UI/Widgets/ScrollView.h"
#include "Aquila/UI/Widgets/TextInput.h"
#include "UI/Inspectors/MaterialComponentUI.h"
#include "UI/Inspectors/MeshComponentUI.h"
#include "UI/Inspectors/ScriptComponentUI.h"
#include "UI/ComponentIcons.h"
#include "UI/Inspectors/ReflectedComponentUI.h"

namespace Editor {

using namespace Aquila;
using namespace Aquila::SceneManagement;
using namespace Aquila::SceneManagement::Components;

namespace {

constexpr std::array<std::string_view, 9> k_component_order = {
	"Transform", "Metadata", "Scene Node", "Mesh", "Material", "Light", "Sky Light", "Camera", "Shadows",
};

}

InspectorPanel::InspectorPanel(GFX::GfxContext &context, UI::Core::TextureCache *texture_cache)
	: m_context(context), m_texture_cache(texture_cache) {}

GFX::GfxTexture *InspectorPanel::icon(std::string_view name) const {
	if (m_texture_cache == nullptr) {
		return nullptr;
	}
	return m_texture_cache->load("Engine/UI/Icons/" + std::string(name) + ".svg");
}

void InspectorPanel::build(UI::Core::View *panel, UI::Core::View *overlay_root) {
	m_tab_strip = panel->find_by_id("props-tabs");
	m_pages = panel->find_by_id("inspector-pages");
	m_scroll_view = panel->find_by_id<UI::Core::ScrollView>("inspector-scroll");
	if (m_tab_strip == nullptr || m_pages == nullptr || m_scroll_view == nullptr) {
		AQUILA_LOG_ERROR("InspectorPanel: 'props-tabs', 'inspector-pages' or 'inspector-scroll' missing from layout");
		return;
	}

	m_empty_state = panel->find_by_id("inspector-empty");
	m_head = panel->find_by_id("props-head");
	m_head_icon = panel->find_by_id<UI::Core::Image>("props-head-icon");
	m_uuid = panel->find_by_id<UI::Core::Label>("inspector-uuid");

	m_name_input = panel->find_by_id<UI::Core::TextInput>("inspector-name");
	if (m_name_input != nullptr) {
		m_name_input->on_submit.connect([this](const std::string &name) {
			if (!m_has_current) {
				return;
			}
			m_current_entity.set_name(name);
			on_entity_renamed(m_current_entity);
		});
	}

	build_component_registry();

	if (overlay_root != nullptr) {
		auto popup = std::make_unique<UI::Core::PopupMenu>();
		m_add_popup = dynamic_cast<UI::Core::PopupMenu *>(overlay_root->add_child(std::move(popup)));
		m_add_popup->set_submenu_icon(icon("chevron-right"));
		m_add_search = m_add_popup->enable_search("Search components...");
		m_add_search->on_changed.connect([this](const std::string &query) { populate_add_menu(query); });
	}

	build_tabs_and_sections();
	clear();
}

Usize InspectorPanel::add_tab(const std::string &title, std::string_view icon_name, bool always_visible) {
	const Usize index = m_tabs.size();

	auto *button = m_tab_strip->add_child<UI::Core::Button>();
	button->add_class("props-tab");
	button->set_icon(icon(icon_name));
	button->set_tooltip(title);
	button->on_click.connect([this, index] { select_tab(index); });

	auto *page = m_pages->add_child<UI::Core::View>();
	page->add_class("props-page");
	page->set_hidden(true);

	auto *sections = page->add_child<UI::Core::View>();
	sections->add_class("props-sections");

	m_tabs.push_back({ .always_visible = always_visible, .button = button, .page = page, .sections = sections });
	return index;
}

void InspectorPanel::add_section(Usize tab, const std::string &component, Unique<IComponentUI> ui) {
	auto *section = m_tabs[tab].sections->add_child<UI::Core::Collapsible>(component);
	section->set_variant("props-panel");
	section->set_state_icons(icon("chevron-right"), icon("chevron-down"), true);
	section->set_icon(icon(component_icon(component)));

	auto *grid = section->add_child<UI::Core::PropertyGrid>();
	grid->set_split(true);
	ui->build(section, grid);
	style_dropdowns(*section);

	if (ui->is_removable()) {
		IComponentUI *raw = ui.get();
		section->add_header_action(icon("x"), "Remove " + component, [this, raw] {
			if (!m_has_current) {
				return;
			}
			raw->remove(m_current_entity);
			show_entity(m_current_entity);
			on_components_changed(m_current_entity);
		});
	}

	m_sections.push_back({ .tab = tab, .collapsible = section, .ui = std::move(ui) });
}

void InspectorPanel::build_tabs_and_sections() {
	const auto &registry = ComponentRegistry::instance();

	auto make_ui = [&](const ComponentDescriptor &descriptor) -> Unique<IComponentUI> {
		if (descriptor.get_name() == "Mesh") {
			return std::make_unique<MeshComponentUI>(descriptor, m_texture_cache);
		}
		if (descriptor.get_name() == "Material") {
			return std::make_unique<MaterialComponentUI>(descriptor, m_context);
		}
		if (descriptor.get_name() == "Script") {
			return std::make_unique<ScriptComponentUI>(descriptor, &m_context);
		}
		return std::make_unique<ReflectedComponentUI>(descriptor, &m_context);
	};

	const Usize object_tab = add_tab("Object", "square", true);
	const Usize components_tab = add_tab("Components", "layers", true);
	const Usize rendering_tab = add_tab("Rendering", "camera", false);
	auto tab_for = [&](ComponentCategory category) {
		switch (category) {
		case ComponentCategory::Object:
			return object_tab;
		case ComponentCategory::Rendering:
			return rendering_tab;
		case ComponentCategory::General:
			break;
		}
		return components_tab;
	};

	for (std::string_view name : k_component_order) {
		const ComponentDescriptor *descriptor = registry.find(name);
		if (descriptor == nullptr) {
			continue;
		}
		Unique<IComponentUI> ui = make_ui(*descriptor);
		if (descriptor->get_name() == "Metadata") {
			ui->on_edited = [this](Entity entity) { on_entity_renamed(entity); };
		}
		add_section(tab_for(descriptor->get_category()), descriptor->get_name(), std::move(ui));
	}
	for (const auto &descriptor : registry.get_all()) {
		if (std::ranges::find(k_component_order, descriptor->get_name()) != k_component_order.end()) {
			continue;
		}
		add_section(tab_for(descriptor->get_category()), descriptor->get_name(), make_ui(*descriptor));
	}
	build_add_button(*m_tabs[components_tab].page);
	select_tab(components_tab);
}

void InspectorPanel::build_add_button(UI::Core::View &page) {
	m_add_button = page.add_child<UI::Core::Button>();
	m_add_button->add_class("props-add-component");
	m_add_button->set_icon(icon("plus"));
	m_add_button->set_text("Add Component");
	m_add_button->set_trailing_icon(icon("chevron-down"));
	m_add_button->on_click.connect([this] { open_add_menu(); });
}

void InspectorPanel::style_dropdowns(UI::Core::View &root) const {
	if (auto *dropdown = dynamic_cast<UI::Core::Dropdown *>(&root)) {
		dropdown->set_variant("props-dropdown");
		dropdown->set_chevron(icon("chevron-down"));
	}
	for (const auto &child : root.get_children()) {
		style_dropdowns(*child);
	}
}

void InspectorPanel::select_tab(Usize tab) {
	if (tab >= m_tabs.size()) {
		return;
	}
	m_active_tab = tab;
	for (Usize i = 0; i < m_tabs.size(); ++i) {
		m_tabs[i].page->set_hidden(i != tab);
		m_tabs[i].button->set_class("props-tab-active", i == tab);
	}
}

bool InspectorPanel::tab_has_content(Usize tab) const {
	if (m_tabs[tab].always_visible) {
		return true;
	}
	return std::ranges::any_of(m_sections, [&](const Section &section) {
		return section.tab == tab && section.ui->matches(m_current_entity);
	});
}

void InspectorPanel::refresh_tabs() {
	for (Usize i = 0; i < m_tabs.size(); ++i) {
		m_tabs[i].button->set_hidden(!tab_has_content(i));
	}
	if (!tab_has_content(m_active_tab)) {
		select_tab(0);
	}
}

void InspectorPanel::show_entity(Entity entity) {
	if (m_scroll_view == nullptr) {
		return;
	}

	m_current_entity = entity;
	m_has_current = true;

	m_empty_state->set_hidden(true);
	m_scroll_view->set_hidden(false);
	m_tab_strip->set_hidden(false);
	if (m_head != nullptr) {
		m_head->set_hidden(false);
	}

	if (m_head_icon != nullptr) {
		m_head_icon->set_texture(icon(entity_icon(entity)));
	}
	if (m_name_input != nullptr) {
		m_name_input->set_text(entity.get_name());
	}
	if (m_uuid != nullptr) {
		const std::string uuid = entity.get_uuid().to_string();
		m_uuid->set_text(uuid.substr(0, 8));
		m_uuid->set_tooltip(uuid);
	}

	for (Section &section : m_sections) {
		const bool has = section.ui->matches(entity);
		section.collapsible->set_hidden(!has);
		if (has) {
			section.ui->show(entity);
		}
	}
	refresh_tabs();
}

void InspectorPanel::refresh_values(Entity entity) {
	if (!m_has_current || entity != m_current_entity) {
		return;
	}
	for (Section &section : m_sections) {
		if (section.ui->matches(entity)) {
			section.ui->refresh();
		}
	}
}

void InspectorPanel::clear() {
	if (m_scroll_view == nullptr) {
		return;
	}
	m_has_current = false;
	m_current_entity = Entity::null();
	if (m_add_popup != nullptr) {
		m_add_popup->dismiss();
	}
	for (auto &section : m_sections) {
		section.collapsible->set_hidden(true);
	}
	if (m_head != nullptr) {
		m_head->set_hidden(true);
	}
	m_tab_strip->set_hidden(true);
	m_scroll_view->set_hidden(true);
	if (m_empty_state != nullptr) {
		m_empty_state->set_hidden(false);
	}
}

void InspectorPanel::build_component_registry() {
	m_categories.push_back({ "Rendering", icon("box") });
	m_categories.push_back({ "Lighting", icon("lightbulb") });
	m_categories.push_back({ "Camera", icon("video") });

	m_addable.push_back({ "Mesh", "Rendering", [](Entity e) { return e.has_component<MeshComponent>(); },
						  [this](Entity e) {
							  e.add_component<MeshComponent>();
							  attach_default_material(e);
						  } });
	m_addable.push_back({ "Material", "Rendering", [](Entity e) { return e.has_component<MaterialComponent>(); },
						  [this](Entity e) { attach_default_material(e); } });
	m_addable.push_back({ "Light", "Lighting", [](Entity e) { return e.has_component<LightComponent>(); },
						  [](Entity e) { e.add_component<LightComponent>(); } });
	m_addable.push_back({ "Sky Light", "Lighting", [](Entity e) { return e.has_component<SkyLightComponent>(); },
						  [](Entity e) { e.add_component<SkyLightComponent>(); } });
	m_addable.push_back({ "Camera", "Camera", [](Entity e) { return e.has_component<CameraComponent>(); },
						  [](Entity e) { e.add_component<CameraComponent>(); } });

	bool has_other = false;
	for (const auto &descriptor : ComponentRegistry::instance().get_all()) {
		const std::string &name = descriptor->get_name();
		if (std::ranges::find(k_component_order, name) != k_component_order.end() || !descriptor->can_add() ||
			!descriptor->is_removable()) {
			continue;
		}
		const ComponentDescriptor *other = descriptor.get();
		m_addable.push_back({ name, "Other", [other](Entity e) { return other->has(e); },
							  [other](Entity e) { other->add(e); } });
		has_other = true;
	}
	if (has_other) {
		m_categories.push_back({ "Other", icon("box") });
	}
}

Ref<Graphics::Material> InspectorPanel::ensure_default_material() {
	if (!m_default_material) {
		m_default_material = SceneManagement::make_default_material(m_context);
	}
	return m_default_material;
}

void InspectorPanel::attach_default_material(Entity entity) {
	if (entity.has_component<MaterialComponent>()) {
		return;
	}
	auto &material = entity.add_component<MaterialComponent>(ensure_default_material());
	material.surface_properties.albedo = Vec4(0.8F, 0.8F, 0.8F, 1.0F);
	material.surface_properties.metallic = 0.0F;
	material.surface_properties.roughness = 0.6F;
}

void InspectorPanel::open_add_menu() {
	if (m_add_button == nullptr) {
		return;
	}
	const Rect rect = m_add_button->get_absolute_rect();
	open_add_popup_at({ rect.position.x, rect.position.y + rect.size.y }, false);
}

void InspectorPanel::open_add_search(Vec2 canvas_pos) {
	open_add_popup_at(canvas_pos, true);
}

void InspectorPanel::open_add_popup_at(Vec2 canvas_pos, bool swallow_first_char) {
	if (!m_has_current || m_add_popup == nullptr) {
		return;
	}
	if (m_add_search != nullptr) {
		m_add_search->set_text("");
	}
	populate_add_menu("");
	m_add_popup->open_at(canvas_pos);
	if (m_add_search != nullptr) {
		m_add_search->request_focus();
		if (swallow_first_char) {
			m_add_search->ignore_next_char();
		}
	}
}

void InspectorPanel::populate_add_menu(const std::string &query) {
	if (m_add_popup == nullptr) {
		return;
	}
	m_add_popup->clear_items();

	std::string needle = query;
	std::ranges::transform(needle, needle.begin(), [](unsigned char c) { return std::tolower(c); });

	auto attach_item = [this](UI::Core::PopupMenu *menu, const AddableComponent &entry) {
		menu->add_item(entry.name, {}, icon(component_icon(entry.name)), [this, name = entry.name, attach = entry.attach] {
			if (!m_has_current) {
				return;
			}
			attach(m_current_entity);
			AQUILA_LOG_INFO("Added {} component to '{}'", name, m_current_entity.get_name());
			show_entity(m_current_entity);
			on_components_changed(m_current_entity);
		});
	};

	if (!needle.empty()) {
		for (const auto &entry : m_addable) {
			if (entry.present(m_current_entity)) {
				continue;
			}
			std::string name = entry.name;
			std::ranges::transform(name, name.begin(), [](unsigned char c) { return std::tolower(c); });
			if (name.find(needle) != std::string::npos) {
				attach_item(m_add_popup, entry);
			}
		}
		m_add_popup->refresh();
		return;
	}

	m_add_popup->add_separator();

	for (const auto &category : m_categories) {
		UI::Core::PopupMenu *submenu = nullptr;
		for (const auto &entry : m_addable) {
			if (entry.category != category.name || entry.present(m_current_entity)) {
				continue;
			}
			if (submenu == nullptr) {
				submenu = m_add_popup->add_submenu(category.name, category.icon);
			}
			attach_item(submenu, entry);
		}
	}

	m_add_popup->refresh();
}

} // namespace Editor
