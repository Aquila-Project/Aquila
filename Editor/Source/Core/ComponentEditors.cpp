#include "Core/ComponentEditors.h"

#include "Aquila/Scene/ComponentRegistry.h"
#include "Aquila/Scene/Components/MaterialComponent.h"
#include "Aquila/Scene/Components/MeshComponent.h"
#include "Aquila/Scene/DefaultScenes.h"
#include "Core/EditorContext.h"
#include "UI/Inspector/MaterialContent.h"
#include "UI/Inspector/MeshContent.h"
#include "UI/Inspector/ScriptContent.h"

namespace Editor {

using Aquila::SceneManagement::ComponentDescriptor;
using Aquila::SceneManagement::ComponentRegistry;
using Aquila::SceneManagement::Entity;
using Aquila::SceneManagement::Components::MaterialComponent;
using Aquila::SceneManagement::Components::MeshComponent;

namespace {

constexpr std::string_view K_OTHER_ICON = "layers";

void attach_default_material(Entity entity, EditorContext &context) {
	if (entity.has_component<MaterialComponent>()) {
		return;
	}
	Aquila::Graphics::MaterialData data;
	data.values["baseColor"] = Vec4(0.8F, 0.8F, 0.8F, 1.0F);
	data.values["roughness"] = 0.6F;
	auto &material = entity.add_component<MaterialComponent>(std::move(data));
	Aquila::SceneManagement::ensure_material(material, context.engine().get_context());
}

template <typename T> Unique<Aquila::UI::Core::IInspectorContent> make_content(Entity entity, EditorContext &context) {
	return std::make_unique<T>(entity, context);
}

}

ComponentEditors::ComponentEditors()
	: m_groups{ { .name = std::string(K_OTHER_GROUP), .icon = std::string(K_OTHER_ICON) } } {}

void ComponentEditors::add(ComponentEditor editor) {
	if (const auto it = std::ranges::find(m_editors, editor.component, &ComponentEditor::component);
		it != m_editors.end()) {
		AQUILA_LOG_WARNING("ComponentEditors: '{}' has an editor already, the last one wins", editor.component);
		*it = std::move(editor);
		return;
	}
	m_editors.push_back(std::move(editor));
}

void ComponentEditors::add_group(ComponentGroup group) {
	if (const auto it = std::ranges::find(m_groups, group.name, &ComponentGroup::name); it != m_groups.end()) {
		AQUILA_LOG_WARNING("ComponentEditors: group '{}' is added twice, the last one wins", group.name);
		*it = std::move(group);
		return;
	}
	m_groups.insert(m_groups.end() - 1, std::move(group));
}

const ComponentEditor *ComponentEditors::find(std::string_view component) const {
	const auto it = std::ranges::find(m_editors, component, &ComponentEditor::component);
	return it == m_editors.end() ? nullptr : &*it;
}

std::string_view ComponentEditors::icon(std::string_view component) const {
	const ComponentEditor *editor = find(component);
	return editor != nullptr ? std::string_view(editor->icon) : K_FALLBACK_ICON;
}

std::string_view ComponentEditors::group_of(std::string_view component) const {
	const ComponentEditor *editor = find(component);
	if (editor == nullptr || std::ranges::find(m_groups, editor->group, &ComponentGroup::name) == m_groups.end()) {
		return K_OTHER_GROUP;
	}
	return editor->group;
}

std::string_view ComponentEditors::entity_icon(Entity entity) const {
	const ComponentEditor *best = nullptr;
	for (const auto &descriptor : ComponentRegistry::instance().get_all()) {
		const ComponentEditor *editor = find(descriptor->get_name());
		if (editor == nullptr || editor->icon_rank <= 0 || (best != nullptr && editor->icon_rank <= best->icon_rank) ||
			!descriptor->has(entity)) {
			continue;
		}
		best = editor;
	}
	return best != nullptr ? std::string_view(best->icon) : K_EMPTY_ENTITY_ICON;
}

std::vector<const ComponentDescriptor *> ComponentEditors::ordered() const {
	std::vector<const ComponentDescriptor *> descriptors;
	for (const auto &descriptor : ComponentRegistry::instance().get_all()) {
		descriptors.push_back(descriptor.get());
	}
	std::ranges::stable_sort(descriptors, {}, [this](const ComponentDescriptor *descriptor) {
		const ComponentEditor *editor = find(descriptor->get_name());
		return editor != nullptr ? std::pair(0, editor->order) : std::pair(1, 0);
	});
	return descriptors;
}

void add_builtin_component_editors(ComponentEditors &editors) {
	editors.add_group({ .name = "Object", .icon = "square" });
	editors.add_group({ .name = "Rendering", .icon = "camera" });
	editors.add_group({ .name = "Scripting", .icon = "file-code" });

	editors.add({ .component = "Transform", .icon = "move-3d", .order = 10, .group = "Object" });
	editors.add({ .component = "Metadata", .icon = "info", .order = 20, .group = "Object" });
	editors.add({ .component = "Scene Node", .icon = "folder-tree", .order = 30, .group = "Object" });
	editors.add({
		.component = "Mesh",
		.icon = "cuboid",
		.order = 40,
		.group = "Rendering",
		.icon_rank = 10,
		.add =
			[](Entity entity, EditorContext &context) {
				entity.get_or_emplace<MeshComponent>();
				attach_default_material(entity, context);
			},
		.content = make_content<MeshContent>,
	});
	editors.add({
		.component = "Material",
		.icon = "eclipse",
		.order = 50,
		.group = "Rendering",
		.add = attach_default_material,
		.content = make_content<MaterialContent>,
	});
	editors.add({ .component = "Light", .icon = "lightbulb", .order = 60, .group = "Rendering", .icon_rank = 20 });
	editors.add({ .component = "Sky Light", .icon = "sun", .order = 70, .group = "Rendering", .icon_rank = 40 });
	editors.add({ .component = "Fog", .icon = "cloud-fog", .order = 80, .group = "Rendering", .icon_rank = 30 });
	editors.add({ .component = "Camera", .icon = "video", .order = 90, .group = "Rendering", .icon_rank = 50 });
	editors.add({ .component = "Fullscreen Effect", .icon = "sparkles", .order = 110, .group = "Rendering" });
	editors.add({ .component = "Script", .order = 200, .group = "Scripting", .content = make_content<ScriptContent> });
}

}
