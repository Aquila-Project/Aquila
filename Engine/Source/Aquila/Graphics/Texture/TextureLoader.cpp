#include "Aquila/Graphics/Texture/TextureLoader.h"
#include "Aquila/Foundation/PrimitiveTypes.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

namespace Aquila::Graphics::Texture {

TextureLoader::RawImageData TextureLoader::load_from_file(const std::string &filepath) {
	return load_from_vfs(filepath);
}

TextureLoader::RawImageData TextureLoader::load_from_vfs(const std::string &filepath) {
	auto file = Platform::Filesystem::VirtualFileSystem::get()->open_file(filepath, AccessMode::Read, OpenMode::Binary);
	if (!file || !file->is_valid()) {
		throw std::runtime_error("Failed to open VFS file: " + filepath);
	}

	Int64 size = file->size();
	if (size <= 0) {
		throw std::runtime_error("VFS file has invalid size: " + filepath);
	}

	std::vector<Uint8> buffer(static_cast<Usize>(size));
	const Usize bytes_read = file->read(buffer.data(), static_cast<Usize>(size));
	if (bytes_read != static_cast<Usize>(size)) {
		throw std::runtime_error("Failed to read complete VFS file: " + filepath);
	}
	return load_from_memory(buffer);
}

TextureLoader::RawImageData TextureLoader::load_from_native(const std::string &native_path) {
	std::ifstream file(native_path, std::ios::binary | std::ios::ate);
	if (!file) {
		throw std::runtime_error("Failed to open file: " + native_path);
	}
	const std::streamsize size = file.tellg();
	if (size <= 0) {
		throw std::runtime_error("File has invalid size: " + native_path);
	}
	std::vector<Uint8> buffer(static_cast<Usize>(size));
	file.seekg(0);
	if (!file.read(reinterpret_cast<char *>(buffer.data()), size)) {
		throw std::runtime_error("Failed to read complete file: " + native_path);
	}
	return load_from_memory(buffer);
}

TextureLoader::RawImageData TextureLoader::load_from_memory(std::span<const Uint8> bytes) {
	RawImageData data;

	int width = 0, height = 0, channels = 0;
	stbi_uc *pixels = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &width, &height, &channels,
											STBI_rgb_alpha);

	data.width = static_cast<Uint32>(width);
	data.height = static_cast<Uint32>(height);
	data.channels = static_cast<Uint32>(channels);

	if (pixels != nullptr) {
		const Usize pixel_size = static_cast<Usize>(data.width) * data.height * 4;
		data.pixels = std::make_unique<Uint8[]>(pixel_size);
		std::memcpy(data.pixels.get(), pixels, pixel_size);
		stbi_image_free(pixels);
	}

	return data;
}

TextureLoader::RawHDRData TextureLoader::load_hdr_from_file(const std::string &filepath) {
	stbi_set_flip_vertically_on_load(0);

	RawHDRData data{};

	auto file = Platform::Filesystem::VirtualFileSystem::get()->open_file(filepath, AccessMode::Read, OpenMode::Binary);
	if (!file || !file->is_valid()) {
		AQUILA_LOG_ERROR("TextureLoader: cannot open '{}'", filepath);
		return data;
	}

	Int64 size = file->size();
	if (size <= 0) {
		AQUILA_LOG_ERROR("TextureLoader: invalid size for '{}'", filepath);
		return data;
	}

	std::vector<char> buffer(size);
	const size_t bytes_read = file->read(buffer.data(), size);
	if (bytes_read != static_cast<size_t>(size)) {
		AQUILA_LOG_ERROR("TextureLoader: incomplete read for '{}'", filepath);
		return data;
	}

	int width = 0, height = 0, channels = 0;
	F32 *pixels = stbi_loadf_from_memory(reinterpret_cast<const stbi_uc *>(buffer.data()), static_cast<int>(bytes_read),
										 &width, &height, &channels, 4);

	data.width = static_cast<Uint32>(width);
	data.height = static_cast<Uint32>(height);
	data.channels = static_cast<Uint32>(channels);

	if (pixels == nullptr) {
		AQUILA_LOG_ERROR("TextureLoader: failed to decode HDR '{}': {}", filepath, stbi_failure_reason());
		return data;
	}

	const size_t pixel_count = static_cast<size_t>(data.width) * data.height * 4;
	data.pixels = std::make_unique<F32[]>(pixel_count);
	std::memcpy(data.pixels.get(), pixels, pixel_count * sizeof(F32));
	stbi_image_free(pixels);

	return data;
}

std::array<Uint8, 4> TextureLoader::color_to_pixel(Vec4 color) {
	return { static_cast<Uint8>(color.r * 255.0F), static_cast<Uint8>(color.g * 255.0F),
			 static_cast<Uint8>(color.b * 255.0F), static_cast<Uint8>(color.a * 255.0F) };
}

} // namespace Aquila::Graphics::Texture
