#ifndef AQUILA_TEXTURELOADER_H
#define AQUILA_TEXTURELOADER_H

#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/RHI/Backend/RHITypes.h"

namespace Aquila::Graphics::Texture {

class TextureLoader {
  public:
	struct ImageData {
		Unique<F32[]> pixels;
		Uint32 width = 0;
		Uint32 height = 0;
		RHI::TextureFormat format = RHI::TextureFormat::RGBA8;
		Uint32 mip_levels = 1;
	};

	struct RawImageData {
		Unique<Uint8[]> pixels;
		Uint32 width = 0;
		Uint32 height = 0;
		Uint32 channels = 0;

		[[nodiscard]] bool is_valid() const { return pixels != nullptr && width > 0 && height > 0; }
		[[nodiscard]] size_t size_bytes() const { return static_cast<size_t>(width) * height * 4; }
	};

	struct RawHDRData {
		Unique<F32[]> pixels;
		Uint32 width = 0;
		Uint32 height = 0;
		Uint32 channels = 0;

		[[nodiscard]] bool is_valid() const { return pixels != nullptr && width > 0 && height > 0; }
		[[nodiscard]] size_t size_bytes() const { return static_cast<size_t>(width) * height * 4 * sizeof(F32); }
	};

	TextureLoader() = default;

	/// Same as load_from_vfs(), the path is a VFS path.
	RawImageData load_from_file(const std::string &filepath);
	/// Always RGBA8, `channels` is what the file had.
	/// Throws when the file can't be read, returns an invalid image when it can't be decoded.
	RawImageData load_from_vfs(const std::string &filepath);
	/// Same as load_from_vfs() for a path on disk. Doesn't touch the VFS, so jobs can call it while mounts change.
	RawImageData load_from_native(const std::string &native_path);
	/// An encoded file (PNG, JPEG, TGA, BMP...) already in memory. Invalid when it can't be decoded. Thread safe.
	RawImageData load_from_memory(std::span<const Uint8> bytes);
	/// RGBA32F. Returns an invalid image on failure.
	RawHDRData load_hdr_from_file(const std::string &filepath);

  private:
	static std::array<Uint8, 4> color_to_pixel(Vec4 color);
};

} // namespace Aquila::Graphics::Texture
#endif
