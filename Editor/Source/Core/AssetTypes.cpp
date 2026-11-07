#include "Core/AssetTypes.h"

#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/Text/StringUtils.h"
#include "Aquila/Graphics/Material/MaterialData.h"
#include "Core/ScriptEditor.h"
#include "UI/Inspector/Material/MaterialAssetContent.h"
#include "UI/Inspector/Mesh/MeshAssetContent.h"
#include "UI/Inspector/Script/ScriptAssetContent.h"
#include "UI/Inspector/Shader/ShaderAssetContent.h"

namespace Editor {

void AssetTypes::add(AssetType type) {
	if (const auto it = std::ranges::find(m_types, type.id, &AssetType::id); it != m_types.end()) {
		AQUILA_LOG_WARNING("AssetTypes: '{}' is added twice, the last one wins", type.id);
		*it = std::move(type);
		return;
	}
	m_types.push_back(std::move(type));
}

const AssetType *AssetTypes::find(std::string_view id) const {
	const auto it = std::ranges::find(m_types, id, &AssetType::id);
	return it == m_types.end() ? nullptr : &*it;
}

const AssetType *AssetTypes::find_for_path(std::string_view path) const {
	const std::string file = Aquila::Foundation::to_lower(path);
	const AssetType *best = nullptr;
	Usize best_length = 0;
	for (const AssetType &type : m_types) {
		for (const std::string &extension : type.extensions) {
			if (extension.size() >= best_length && file.ends_with(extension)) {
				best = &type;
				best_length = extension.size();
			}
		}
	}
	return best;
}

void add_builtin_asset_types(AssetTypes &types) {
	types.add({
		.id = "texture",
		.title = "Texture",
		.icon = "image",
		.extensions = { ".png", ".jpg", ".jpeg", ".tga", ".bmp", ".hdr", ".svg" },
		.thumbnail = AssetThumbnail::Image,
	});
	types.add({
		.id = "model",
		.title = "Model",
		.icon = "box",
		.extensions = { ".gltf", ".glb", ".obj", ".fbx" },
		.inspect = inspect_mesh_asset,
	});
	types.add({
		.id = "material",
		.title = "Material",
		.icon = "eclipse",
		.extensions = { std::string(Aquila::Graphics::MATERIAL_EXTENSION) },
		.thumbnail = AssetThumbnail::Material,
		.inspect = inspect_material_asset,
	});
	types.add({
		.id = "shader",
		.title = "Shader",
		.icon = "file-code",
		.extensions = { ".slang", ".hlsl", ".glsl" },
		.open = open_in_script_editor,
		.inspect = inspect_shader_asset,
	});
	types.add({
		.id = "script",
		.title = "Script",
		.icon = "file-code",
		.extensions = { ".luau", ".lua" },
		.open = open_in_script_editor,
		.inspect = inspect_script_asset,
	});
	types.add({
		.id = "definitions",
		.title = "Script Definitions",
		.icon = "file-code",
		.extensions = { ".d.luau" },
		.open = open_in_script_editor,
	});
	types.add({
		.id = "scene",
		.title = "Scene",
		.extensions = { ".aqscene" },
	});
	types.add({
		.id = "ui",
		.title = "UI Markup",
		.icon = "panel-left",
		.extensions = { ".aqlayout", ".aqstyle" },
	});
}

}
