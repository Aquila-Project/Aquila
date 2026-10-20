#ifndef AQUILA_GRAPHICS_MATERIAL_ASSETS_H
#define AQUILA_GRAPHICS_MATERIAL_ASSETS_H

#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Foundation/Signal.h"
#include "Aquila/Foundation/Singleton.h"
#include "Aquila/Graphics/Material/Material.h"

namespace Aquila::GFX {
class GfxContext;
}

namespace Aquila::Graphics {

/// The materials of .aqmat files, one shared Material per file. Main thread only.
///
/// @code
/// Ref<Material> brick = MaterialAssets::get()->load(ctx, "/project/Content/Materials/Brick.aqmat");
/// brick->set_value("roughness", 0.8F);
/// MaterialAssets::get()->save(*brick);
/// @endcode
class MaterialAssets : public Foundation::Singleton<MaterialAssets> {
  public:
	/// Loads once, later calls return the same material until update() replaces it, so callers that keep the result
	/// should call load() again every frame (it's a lookup) to pick up a replacement. A missing or broken file gives
	/// the standard material.
	Ref<Material> load(GFX::GfxContext &ctx, const std::string &asset_path);
	/// A material for `data` that nobody shares (an entity's own). When `data.shader` doesn't compile it falls back
	/// to the standard shader but keeps `data`, so fixing the shader and reloading brings it back.
	Ref<Material> create(GFX::GfxContext &ctx, const MaterialData &data);
	/// Gives an asset new data and writes the file. Parameter changes update the shared material in place, a new
	/// shader, double sided or blend mode builds a new one (returned, and handed out by load() from now on).
	Ref<Material> update(GFX::GfxContext &ctx, const std::string &asset_path, const MaterialData &data);
	/// Writes the material's data to its .aqmat. False when it has no asset path or the write fails.
	bool save(const Material &material);

	/// Forgets every loaded asset, for when the project changes.
	void clear() { m_assets.clear(); }
	/// Starts loading, on the JobSystem, every texture the .aqmat files under `directory` (a VFS folder, searched
	/// recursively) use, so they're ready or close by the time something draws them. Reads the files, compiles nothing.
	/// @return how many textures it queued
	Usize preload_textures(const std::string &directory);

	/// Fires after update() or save() wrote an asset, with its path. Thumbnails listen to it.
	Signal<void(const std::string &)> on_changed;

  private:
	std::unordered_map<std::string, Ref<Material>> m_assets;
};

}

#endif
