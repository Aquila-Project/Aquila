#include "UI/Inspector/Shader/ShaderAssetContent.h"

#include "Aquila/Graphics/Material/MaterialFactory.h"
#include "Aquila/Graphics/Shader/ShaderSource.h"
#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"
#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/MessageBox.h"
#include "Aquila/UI/Widgets/PropertyGrid.h"
#include "Core/EditorContext.h"
#include "Core/ScriptEditor.h"
#include "UI/Inspector/Common/AssetInspectable.h"
#include "UI/Inspector/Common/ScriptEditorButton.h"
#include "UI/Inspector/Material/MaterialParameterRows.h"

namespace Editor {

using namespace Aquila;

ShaderAssetContent::ShaderAssetContent(std::string path, EditorContext &context)
	: m_path(std::move(path)), m_context(context) {}

ShaderAssetContent::~ShaderAssetContent() = default;

void ShaderAssetContent::build_below(UI::Core::PropertyGrid &grid) {
	const std::string code = Platform::Filesystem::VirtualFileSystem::get()->read_text_file(m_path);
	const Graphics::Shader::ShaderDomain domain = Graphics::Shader::detect_shader_domain(code);
	grid.add_row<UI::Core::Label>("Kind", std::string(Graphics::Shader::domain_name(domain)) + " shader");
	if (UI::Core::Button *open = add_script_editor_button(grid, m_context)) {
		open->on_click.connect([this] { open_in_script_editor(m_path); });
	}

	GFX::GfxContext &gfx = m_context.engine().get_context();
	Graphics::MaterialData data;
	data.shader = m_path;
	m_material = Graphics::MaterialFactory::get()->create(gfx, data);
	if (m_material == nullptr) {
		UI::Core::create_message_box(grid, UI::Core::MessageKind::Error,
									 "Doesn't compile as a material, see the console");
		return;
	}
	m_rows = std::make_unique<MaterialParameterRows>(grid, &gfx, true);
	m_rows->show(m_material.get());
}

void ShaderAssetContent::refresh() {
	if (m_rows != nullptr) {
		m_rows->refresh();
	}
}

Option<UI::Core::Inspectable> inspect_shader_asset(const std::string &path, EditorContext &context) {
	UI::Core::InspectorSection shader;
	shader.id = "shader";
	shader.title = "Shader";
	shader.icon = asset_icon(path, context);
	shader.content = std::make_unique<ShaderAssetContent>(path, context);

	std::vector<UI::Core::InspectorSection> sections;
	sections.push_back(std::move(shader));
	return make_asset_inspectable(path, context, std::move(sections));
}

}
