#include "UI/Inspector/Script/ScriptAssetContent.h"

#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/PropertyGrid.h"
#include "Core/ScriptEditor.h"
#include "UI/Inspector/Common/AssetInspectable.h"
#include "UI/Inspector/Common/ScriptEditorButton.h"
#include "UI/Inspector/Script/ScriptFieldRows.h"

namespace Editor {

using namespace Aquila;

ScriptAssetContent::ScriptAssetContent(std::string path, EditorContext &context)
	: m_script{ .path = std::move(path) }, m_context(context) {}

ScriptAssetContent::~ScriptAssetContent() = default;

void ScriptAssetContent::build_below(UI::Core::PropertyGrid &grid) {
	if (UI::Core::Button *open = add_script_editor_button(grid, m_context)) {
		open->on_click.connect([this] { open_in_script_editor(m_script.path); });
	}
	m_rows = std::make_unique<ScriptFieldRows>(grid, m_context, true);
	m_rows->show(&m_script);
}

void ScriptAssetContent::refresh() {
	if (m_rows != nullptr) {
		m_rows->refresh();
	}
}

Option<UI::Core::Inspectable> inspect_script_asset(const std::string &path, EditorContext &context) {
	UI::Core::InspectorSection script;
	script.id = "script";
	script.title = "Script";
	script.icon = asset_icon(path, context);
	script.content = std::make_unique<ScriptAssetContent>(path, context);

	std::vector<UI::Core::InspectorSection> sections;
	sections.push_back(std::move(script));
	return make_asset_inspectable(path, context, std::move(sections));
}

}
