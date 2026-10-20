#ifndef AQUILA_EDITOR_CORE_ASSET_THUMBNAILS_H
#define AQUILA_EDITOR_CORE_ASSET_THUMBNAILS_H

#include "Aquila/Foundation/Defines.h"
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Foundation/Signal.h"
#include "Aquila/Foundation/Timer.h"

namespace Aquila::GFX {
class GfxContext;
class GfxTexture;
}

namespace Aquila::Rendering {
class MaterialPreview;
}

namespace Aquila::UI::Core {
class TextureCache;
}

namespace Editor {

/// Thumbnails of content files: an image is a small copy of its picture, decoded and shrunk on the JobSystem (SVGs go
/// through the UI TextureCache), a .aqmat is its material rendered on a sphere, two per frame in update(). get() is
/// null until one is ready and on_ready fires then (asset slots refresh themselves). Edited materials render again a
/// moment after the last edit, and so do materials whose textures finished loading. Hot reloaded textures refresh
/// their image and every material thumbnail. Main thread only.
class AssetThumbnails {
  public:
	AssetThumbnails(Aquila::GFX::GfxContext &gfx, Aquila::UI::Core::TextureCache &textures);
	~AssetThumbnails();

	AQUILA_NONCOPYABLE(AssetThumbnails)
	AQUILA_NONMOVEABLE(AssetThumbnails);

	/// Null when the file has no thumbnail or it isn't rendered yet. A material keeps the same texture when it
	/// renders again, an image gets a new one after a hot reload (on_ready tells).
	[[nodiscard]] Aquila::GFX::GfxTexture *get(const std::string &path);
	/// Uploads decoded images and renders due materials. EditorModule calls it every frame before the windows update.
	void update();
	/// Forgets everything, for when the project changes. Views still showing a thumbnail must be rebuilt.
	void clear();

	Signal<void(const std::string &path)> on_ready;

  private:
	struct Pending {
		std::string path;
		Aquila::Foundation::TimePoint due;
	};

	struct DecodedImage {
		std::string path;
		Uint64 generation = 0;
		Uint32 width = 0;
		Uint32 height = 0;
		std::vector<std::vector<Uint8>> mips;
	};

	struct Inbox {
		std::mutex mutex;
		std::vector<DecodedImage> done;
	};

	void queue(const std::string &path, F64 delay_seconds);
	void render(const std::string &path);
	void request_image(const std::string &path);
	void upload_images();
	void on_texture_loaded(const std::string &path);
	void on_texture_reloaded(const std::string &path);
	void announce(const std::string &path);

	Aquila::GFX::GfxContext &m_gfx;
	Aquila::UI::Core::TextureCache &m_textures;
	Unique<Aquila::Rendering::MaterialPreview> m_preview;
	std::unordered_map<std::string, Ref<Aquila::GFX::GfxTexture>> m_materials;
	std::unordered_map<std::string, Ref<Aquila::GFX::GfxTexture>> m_images;
	std::unordered_set<std::string> m_image_requests;
	std::unordered_set<std::string> m_vector_images;
	Ref<Inbox> m_inbox = std::make_shared<Inbox>();
	Uint64 m_generation = 0;
	std::vector<Pending> m_queue;
	Signal<void(const std::string &)>::Connection m_material_connection;
	Signal<void(const std::string &)>::Connection m_texture_connection;
	Signal<void(const std::string &)>::Connection m_loaded_connection;
};

}

#endif
