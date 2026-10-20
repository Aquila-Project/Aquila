#ifndef AQUILA_GRAPHICS_TEXTURE_LIBRARY_H
#define AQUILA_GRAPHICS_TEXTURE_LIBRARY_H

#include "Aquila/Foundation/JobSystem.h"
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Foundation/Signal.h"
#include "Aquila/Foundation/Singleton.h"
#include "Aquila/GFX/GfxTexture.h"

namespace Aquila::GFX {
class GfxContext;
}

namespace Aquila::Graphics {

/// 1x1 textures bound to texture parameters nobody set: white for colors and masks, a flat normal for normal maps.
/// Gray and Black stand in for color and emission maps that are still loading.
enum class DefaultTexture : Uint8 { White, Black, FlatNormal, Gray };

/// Material textures, loaded once per file with a full mip chain. Color textures (`srgb`) are decoded to linear by
/// the sampler, data textures (normals, roughness) aren't, so the same file can be cached twice.
///
/// Loading is asynchronous: files are read, decoded and mipped on the JobSystem, update() uploads the finished ones
/// on the main thread and bumps get_revision(), and materials swap their placeholders for them on their next flush().
/// Main thread only, apart from what runs inside the jobs.
class TextureLibrary : public Foundation::Singleton<TextureLibrary> {
  public:
	TextureLibrary();
	~TextureLibrary();

	AQUILA_NONCOPYABLE(TextureLibrary);
	AQUILA_NONMOVEABLE(TextureLibrary);

	/// `path` is a VFS path. The texture when it's ready, otherwise null: it starts loading (once) and the caller
	/// binds a placeholder until get_revision() moves. Also null when the file can't be read or decoded (it logs once
	/// and doesn't retry until the file changes).
	Ref<GFX::GfxTexture> load(GFX::GfxContext &ctx, const std::string &path, bool srgb);
	/// Starts loading without waiting for anyone to ask. Does nothing when it's loaded, loading or failed.
	void preload(const std::string &path, bool srgb, Foundation::JobPriority priority = Foundation::JobPriority::Low);
	[[nodiscard]] bool is_loading(const std::string &path, bool srgb) const;
	[[nodiscard]] Usize get_loading_count() const { return m_loading.size(); }

	/// Uploads what the jobs finished, a few dozen MB per call, and asks for another frame when more is waiting.
	/// Application calls it at the start of every frame.
	/// @return true when a texture became ready or was reloaded
	bool update(GFX::GfxContext &ctx);
	/// Blocks until every texture that's loading is ready (or failed) and uploaded. For games, which would rather
	/// start a moment later than show placeholders. Main thread only.
	void finish_loading(GFX::GfxContext &ctx);

	GFX::GfxTexture &get_default(GFX::GfxContext &ctx, DefaultTexture kind);
	/// Average color of a loaded texture, linear (sRGB ones are decoded first). Nullopt when it isn't loaded yet, or
	/// failed.
	[[nodiscard]] Option<Vec4> get_average(const std::string &path, bool srgb) const;

	/// Drops everything that isn't in use, for when the project changes. Textures materials still hold stay alive,
	/// loads still running are thrown away when they finish.
	void clear();

	/// Hot reload: decodes again every cached texture whose file changed on disk and forgets failures whose file
	/// changed, so the next load() retries them. Application calls it every 0.5 s outside frames while shader hot
	/// reload is on. The new textures arrive through update() like any load, the old ones die once the GPU is done.
	/// @return true when a failed texture can be retried
	bool poll_changes(GFX::GfxContext &ctx);
	/// Bumped whenever a texture becomes ready or is reloaded, materials compare it to know their textures are stale.
	[[nodiscard]] Uint64 get_revision() const { return m_revision; }

	/// Fires from update() after a texture finished loading for the first time, with its VFS path.
	Signal<void(const std::string &)> on_loaded;
	/// Fires from update() after a hot reload replaced a texture, with its VFS path. For caches that show the file
	/// (thumbnails).
	Signal<void(const std::string &)> on_reloaded;

  private:
	using Key = std::pair<std::string, bool>;

	struct Entry {
		Ref<GFX::GfxTexture> texture;
		Uint64 write_time = 0;
		Vec4 average = Vec4(1.F);
	};

	struct Decoded;
	struct Inbox;

	/// Runs inside a job: reads, decodes and mips one file. Reads `native_path` when the VFS path maps to one, so
	/// the job doesn't depend on the mounts. Invalid when it can't.
	[[nodiscard]] static Decoded decode(const std::string &path, const Option<std::string> &native_path, bool srgb);
	void start(const Key &key, Foundation::JobPriority priority);
	[[nodiscard]] Ref<GFX::GfxTexture> upload(GFX::GfxContext &ctx, const std::string &path, Decoded &decoded) const;

	std::map<Key, Entry> m_textures;
	std::map<Key, Uint64> m_failed;
	std::set<Key> m_loading;
	Ref<Inbox> m_inbox;
	Uint64 m_generation = 0;
	std::array<Ref<GFX::GfxTexture>, 4> m_defaults;
	Uint64 m_revision = 0;
};

} // namespace Aquila::Graphics

#endif
