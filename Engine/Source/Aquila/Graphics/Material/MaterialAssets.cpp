#include "Aquila/Graphics/Material/MaterialAssets.h"
#include "Aquila/Foundation/Macros.h"
#include "Aquila/Graphics/Material/MaterialFactory.h"
#include "Aquila/Graphics/Texture/TextureLibrary.h"
#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"

namespace Aquila::Graphics {

Usize MaterialAssets::preload_textures(const std::string &directory) {
	const Option<std::string> native_root = Platform::Filesystem::VirtualFileSystem::get()->to_native_path(directory);
	std::error_code error;
	if (!native_root || !std::filesystem::is_directory(*native_root, error)) {
		return 0;
	}

	Usize queued = 0;
	TextureLibrary *textures = TextureLibrary::get();
	for (auto it = std::filesystem::recursive_directory_iterator(
			 *native_root, std::filesystem::directory_options::skip_permission_denied, error);
		 !error && it != std::filesystem::recursive_directory_iterator(); it.increment(error)) {
		if (!it->is_regular_file(error) || it->path().extension() != MATERIAL_EXTENSION) {
			continue;
		}
		const std::string relative = std::filesystem::relative(it->path(), *native_root, error).generic_string();
		MaterialData data;
		if (relative.empty() || !load_material_file(directory + "/" + relative, data)) {
			continue;
		}
		for (const auto &[name, value] : data.values) {
			const auto *reference = std::get_if<TextureReference>(&value);
			if (reference != nullptr && !reference->path.empty()) {
				const bool srgb = is_srgb_texture_name(name);
				if (!textures->is_loading(reference->path, srgb)) {
					textures->preload(reference->path, srgb);
					queued += textures->is_loading(reference->path, srgb) ? 1 : 0;
				}
			}
		}
	}
	return queued;
}

Ref<Material> MaterialAssets::load(GFX::GfxContext &ctx, const std::string &asset_path) {
	if (auto it = m_assets.find(asset_path); it != m_assets.end()) {
		return it->second;
	}

	MaterialData data;
	if (!load_material_file(asset_path, data)) {
		data = MaterialData{};
	}
	Ref<Material> material = create(ctx, data);
	if (material) {
		material->set_asset_path(asset_path);
		material->name = std::filesystem::path(asset_path).stem().string();
	}
	m_assets[asset_path] = material;
	return material;
}

Ref<Material> MaterialAssets::create(GFX::GfxContext &ctx, const MaterialData &data) {
	Ref<Material> material = MaterialFactory::get()->create(ctx, data);
	if (material) {
		return material;
	}

	AQUILA_LOG_WARNING("Material: '{}' doesn't compile, drawing with {} until it does", data.shader, STANDARD_SHADER);
	MaterialData fallback = data;
	fallback.shader = std::string(STANDARD_SHADER);
	material = MaterialFactory::get()->create(ctx, fallback);
	if (material) {
		material->apply(data);
	}
	return material;
}

Ref<Material> MaterialAssets::update(GFX::GfxContext &ctx, const std::string &asset_path, const MaterialData &data) {
	Ref<Material> material = load(ctx, asset_path);
	const bool same_pipeline = material && material->get_data().shader == data.shader &&
		material->get_data().double_sided == data.double_sided &&
		material->get_data().blend_mode == data.blend_mode;
	if (same_pipeline) {
		material->apply(data);
	} else {
		material = create(ctx, data);
		if (material) {
			material->set_asset_path(asset_path);
			material->name = std::filesystem::path(asset_path).stem().string();
		}
		m_assets[asset_path] = material;
	}
	save_material_file(asset_path, data);
	on_changed(asset_path);
	return material;
}

bool MaterialAssets::save(const Material &material) {
	if (material.get_asset_path().empty()) {
		return false;
	}
	const bool saved = save_material_file(material.get_asset_path(), material.get_data());
	if (saved) {
		on_changed(material.get_asset_path());
	}
	return saved;
}

}
