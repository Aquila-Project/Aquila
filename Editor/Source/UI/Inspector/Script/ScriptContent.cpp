#include "UI/Inspector/Script/ScriptContent.h"

#include "Aquila/Scene/Components/ScriptComponent.h"
#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/PropertyGrid.h"
#include "Core/EditorContext.h"
#include "Core/ScriptEditor.h"
#include "UI/Inspector/Common/ScriptEditorButton.h"
#include "UI/Inspector/Script/ScriptFieldRows.h"

namespace Editor {

using namespace Aquila;
using SceneManagement::Entity;
using SceneManagement::Components::ScriptComponent;

ScriptContent::ScriptContent(Entity entity, EditorContext &context) : m_entity(entity), m_context(context) {}

ScriptContent::~ScriptContent() = default;

void ScriptContent::build_below(UI::Core::PropertyGrid &grid) {
	m_open_button = add_script_editor_button(grid, m_context);
	if (m_open_button != nullptr) {
		m_open_button->on_click.connect([this] { open_script(); });
	}
	m_rows = std::make_unique<ScriptFieldRows>(grid, m_context, false);
	show_fields();
}

void ScriptContent::refresh() {
	if (m_rows != nullptr) {
		m_rows->refresh();
	}
}

void ScriptContent::on_property_edited() {
	show_fields();
}

void ScriptContent::open_script() const {
	const auto *component = m_entity.exists() ? m_entity.try_get_component<ScriptComponent>() : nullptr;
	if (component != nullptr && !component->path.empty()) {
		open_in_script_editor(component->path);
	}
}

void ScriptContent::show_fields() {
	if (m_rows == nullptr) {
		return;
	}
	auto *component = m_entity.exists() ? m_entity.try_get_component<ScriptComponent>() : nullptr;
	if (m_open_button != nullptr) {
		m_open_button->set_enabled(component != nullptr && !component->path.empty());
	}
	m_rows->show(component, [this] {
		if (notify_edited) {
			notify_edited();
		}
	});
}

}
