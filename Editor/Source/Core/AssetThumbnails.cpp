#include "Core/AssetThumbnails.h"

#include "Aquila/Foundation/FrameScheduler.h"
#include "Aquila/Foundation/JobSystem.h"
#include "Aquila/GFX/GfxContext.h"
#include "Aquila/Graphics/Material/MaterialAssets.h"
#include "Aquila/Graphics/Texture/TextureLibrary.h"
#include "Aquila/Graphics/Texture/TextureLoader.h"
#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"
#include "Aquila/Rendering/MaterialPreview.h"
#include "Aquila/UI/Core/TextureCache.h"
#include "Aquila/UI/Widgets/AssetSlot.h"

#include <initializer_list>

namespace Editor {

using Aquila::Platform::Filesystem::VirtualFileSystem;

namespace {

constexpr Uint32 k_material_size = 128;
constexpr Usize k_renders_per_frame = 2;
constexpr F64 k_edit_delay = 0.3;
constexpr Int64 k_max_image_bytes = 64 * 1024 * 1024;
constexpr Uint32 k_image_size = 256;

std::string lowered(std::string_view path) {
	std::string file(path);
	std::ranges::transform(file, file.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return file;
}

bool is_image(std::string_view path) {
	const std::string file = lowered(path);
	return std::ranges::any_of(std::initializer_list<std::string_view>{ ".png", ".jpg", ".jpeg", ".tga", ".bmp", ".svg" },
							   [&file](std::string_view extension) { return file.ends_with(extension); });
}

bool is_vector(std::string_view path) {
	return lowered(path).ends_with(".svg");
}

std::vector<Uint8> halve(const std::vector<Uint8> &source, Uint32 width, Uint32 height) {
	const Uint32 next_width = std::max(1U, width / 2U);
	const Uint32 next_height = std::max(1U, height / 2U);
	std::vector<Uint8> target(static_cast<Usize>(next_width) * next_height * 4U);
	for (Uint32 y = 0; y < next_height; ++y) {
		for (Uint32 x = 0; x < next_width; ++x) {
			std::array<F32, 3> premultiplied{};
			F32 alpha = 0.F;
			for (Uint32 dy = 0; dy < 2; ++dy) {
				for (Uint32 dx = 0; dx < 2; ++dx) {
					const Uint32 sx = std::min(width - 1, (x * 2U) + dx);
					const Uint32 sy = std::min(height - 1, (y * 2U) + dy);
					const Uint8 *texel = &source[((static_cast<Usize>(sy) * width) + sx) * 4U];
					const F32 a = static_cast<F32>(texel[3]) / 255.F;
					for (Usize c = 0; c < 3; ++c) {
						premultiplied[c] += static_cast<F32>(texel[c]) * a;
					}
					alpha += a;
				}
			}
			Uint8 *out = &target[((static_cast<Usize>(y) * next_width) + x) * 4U];
			for (Usize c = 0; c < 3; ++c) {
				out[c] = alpha > 0.F ? static_cast<Uint8>(std::lround(premultiplied[c] / alpha)) : 0U;
			}
			out[3] = static_cast<Uint8>(std::lround(alpha * 0.25F * 255.F));
		}
	}
	return target;
}

std::vector<std::vector<Uint8>> thumbnail_mips(const Uint8 *pixels, Uint32 &width, Uint32 &height) {
	std::vector<Uint8> level(pixels, pixels + (static_cast<Usize>(width) * height * 4U));
	while (std::max(width, height) > k_image_size) {
		level = halve(level, width, height);
		width = std::max(1U, width / 2U);
		height = std::max(1U, height / 2U);
	}
	std::vector<std::vector<Uint8>> mips;
	mips.push_back(std::move(level));
	Uint32 w = width;
	Uint32 h = height;
	while (w > 1 || h > 1) {
		mips.push_back(halve(mips.back(), w, h));
		w = std::max(1U, w / 2U);
		h = std::max(1U, h / 2U);
	}
	return mips;
}

bool uses_texture(const Aquila::Graphics::MaterialData &data, const std::string &texture_path) {
	return std::ranges::any_of(data.values, [&texture_path](const auto &entry) {
		const auto *reference = std::get_if<Aquila::Graphics::TextureReference>(&entry.second);
		return reference != nullptr && reference->path == texture_path;
	});
}

}

AssetThumbnails::AssetThumbnails(Aquila::GFX::GfxContext &gfx, Aquila::UI::Core::TextureCache &textures)
	: m_gfx(gfx), m_textures(textures),
	  m_preview(std::make_unique<Aquila::Rendering::MaterialPreview>(gfx, k_material_size)) {
	m_material_connection =
		Aquila::Graphics::MaterialAssets::get()->on_changed.connect([this](const std::string &path) {
			if (m_materials.contains(path)) {
				queue(path, k_edit_delay);
			}
		});
	m_texture_connection = Aquila::Graphics::TextureLibrary::get()->on_reloaded.connect(
		[this](const std::string &path) { on_texture_reloaded(path); });
	m_loaded_connection = Aquila::Graphics::TextureLibrary::get()->on_loaded.connect(
		[this](const std::string &path) { on_texture_loaded(path); });
}

AssetThumbnails::~AssetThumbnails() {
	Aquila::Graphics::MaterialAssets::get()->on_changed.disconnect(m_material_connection);
	Aquila::Graphics::TextureLibrary::get()->on_reloaded.disconnect(m_texture_connection);
	Aquila::Graphics::TextureLibrary::get()->on_loaded.disconnect(m_loaded_connection);
}

Aquila::GFX::GfxTexture *AssetThumbnails::get(const std::string &path) {
	if (is_image(path)) {
		if (is_vector(path)) {
			Aquila::GFX::GfxTexture *texture = m_textures.load(path);
			if (texture != nullptr) {
				m_vector_images.insert(path);
			}
			return texture;
		}
		if (auto found = m_images.find(path); found != m_images.end()) {
			return found->second.get();
		}
		request_image(path);
		return nullptr;
	}
	if (!path.ends_with(Aquila::Graphics::MATERIAL_EXTENSION)) {
		return nullptr;
	}
	auto [it, inserted] = m_materials.try_emplace(path);
	if (inserted) {
		queue(path, 0.0);
	}
	return it->second.get();
}

void AssetThumbnails::queue(const std::string &path, F64 delay_seconds) {
	const auto due = Aquila::Foundation::now() +
		std::chrono::duration_cast<Aquila::Foundation::Clock::duration>(std::chrono::duration<F64>(delay_seconds));
	auto it = std::ranges::find(m_queue, path, &Pending::path);
	if (it != m_queue.end()) {
		it->due = due;
	} else {
		m_queue.push_back({ .path = path, .due = due });
	}
	if (delay_seconds > 0.0) {
		Aquila::Foundation::FrameScheduler::get()->request_frame_in(delay_seconds);
	} else {
		Aquila::Foundation::FrameScheduler::get()->request_frame();
	}
}

void AssetThumbnails::request_image(const std::string &path) {
	auto *vfs = VirtualFileSystem::get();
	if (m_image_requests.contains(path) || vfs->get_file_size(path) > k_max_image_bytes) {
		return;
	}
	m_image_requests.insert(path);

	auto job = [inbox = m_inbox, path, generation = m_generation, native_path = vfs->to_native_path(path)] {
		DecodedImage image{ .path = path, .generation = generation };
		Aquila::Graphics::Texture::TextureLoader::RawImageData raw;
		try {
			raw = native_path ? Aquila::Graphics::Texture::TextureLoader().load_from_native(*native_path)
							  : Aquila::Graphics::Texture::TextureLoader().load_from_vfs(path);
		} catch (const std::exception &error) {
			AQUILA_LOG_WARNING("AssetThumbnails: {}", error.what());
		}
		if (raw.is_valid()) {
			image.width = raw.width;
			image.height = raw.height;
			image.mips = thumbnail_mips(raw.pixels.get(), image.width, image.height);
		}
		{
			std::scoped_lock lock(inbox->mutex);
			inbox->done.push_back(std::move(image));
		}
		Aquila::Foundation::FrameScheduler::get()->request_frame();
	};
	Aquila::Foundation::JobSystem::get()->schedule(Aquila::Foundation::JobPriority::Medium, "Thumbnail " + path,
												   std::move(job));
}

void AssetThumbnails::upload_images() {
	std::vector<DecodedImage> done;
	{
		std::scoped_lock lock(m_inbox->mutex);
		done.swap(m_inbox->done);
	}
	for (DecodedImage &image : done) {
		if (image.generation != m_generation || image.mips.empty()) {
			continue;
		}
		Aquila::RHI::TextureDesc desc{};
		desc.width = image.width;
		desc.height = image.height;
		desc.format = Aquila::RHI::TextureFormat::RGBA8;
		desc.usage = Aquila::RHI::TextureUsage::Sampled | Aquila::RHI::TextureUsage::TransferDst;
		desc.mip_levels = static_cast<Uint32>(image.mips.size());
		desc.sampler = Aquila::RHI::SamplerDesc::ui_image(static_cast<F32>(image.mips.size()));
		desc.debug_name = image.path + " (thumbnail)";
		Ref<Aquila::GFX::GfxTexture> texture = m_gfx.create_texture(desc);
		if (!texture) {
			continue;
		}
		m_gfx.upload_texture_mips(*texture, image.mips);
		m_images[image.path] = std::move(texture);
		announce(image.path);
	}
}

void AssetThumbnails::update() {
	upload_images();
	if (m_queue.empty()) {
		return;
	}
	const auto current = Aquila::Foundation::now();
	std::vector<std::string> due;
	for (auto it = m_queue.begin(); it != m_queue.end() && due.size() < k_renders_per_frame;) {
		if (it->due <= current) {
			due.push_back(std::move(it->path));
			it = m_queue.erase(it);
		} else {
			++it;
		}
	}
	for (const std::string &path : due) {
		render(path);
	}

	if (m_queue.empty()) {
		return;
	}
	const auto next = std::ranges::min(m_queue, {}, &Pending::due).due;
	const F64 wait = std::chrono::duration<F64>(next - Aquila::Foundation::now()).count();
	if (wait <= 0.0) {
		Aquila::Foundation::FrameScheduler::get()->request_frame();
	} else {
		Aquila::Foundation::FrameScheduler::get()->request_frame_in(wait);
	}
}

void AssetThumbnails::render(const std::string &path) {
	auto found = m_materials.find(path);
	if (found == m_materials.end() || !VirtualFileSystem::get()->exists(path)) {
		return;
	}
	Ref<Aquila::Graphics::Material> material = Aquila::Graphics::MaterialAssets::get()->load(m_gfx, path);
	if (!material) {
		return;
	}
	Ref<Aquila::GFX::GfxTexture> texture = m_preview->render(*material, found->second);
	if (!texture) {
		return;
	}
	found->second = std::move(texture);
	announce(path);
	Aquila::Foundation::FrameScheduler::get()->request_frame();
}

void AssetThumbnails::on_texture_loaded(const std::string &path) {
	for (const auto &[material_path, texture] : m_materials) {
		Ref<Aquila::Graphics::Material> material = Aquila::Graphics::MaterialAssets::get()->load(m_gfx, material_path);
		if (material && uses_texture(material->get_data(), path)) {
			queue(material_path, k_edit_delay);
		}
	}
}

void AssetThumbnails::on_texture_reloaded(const std::string &path) {
	if (m_images.contains(path) || m_image_requests.contains(path)) {
		m_image_requests.erase(path);
		request_image(path);
	}
	if (m_vector_images.contains(path)) {
		if (m_textures.reload(path) == nullptr) {
			m_vector_images.erase(path);
		}
		announce(path);
	}
	for (const auto &[material, texture] : m_materials) {
		queue(material, 0.0);
	}
}

void AssetThumbnails::announce(const std::string &path) {
	on_ready(path);
	Aquila::UI::Core::AssetSlot::refresh_thumbnails(path);
}

void AssetThumbnails::clear() {
	m_materials.clear();
	m_images.clear();
	m_image_requests.clear();
	m_vector_images.clear();
	m_queue.clear();
	++m_generation;
	std::scoped_lock lock(m_inbox->mutex);
	m_inbox->done.clear();
}

}
