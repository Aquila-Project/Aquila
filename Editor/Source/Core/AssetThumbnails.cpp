#include "Core/AssetThumbnails.h"

#include "Aquila/Foundation/FrameScheduler.h"
#include "Aquila/Foundation/JobSystem.h"
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Foundation/Text/StringUtils.h"
#include "Aquila/GFX/GfxContext.h"
#include "Aquila/Graphics/Material/MaterialAssets.h"
#include "Aquila/Graphics/Texture/MipChain.h"
#include "Aquila/Graphics/Texture/TextureLibrary.h"
#include "Aquila/Graphics/Texture/TextureLoader.h"
#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"
#include "Aquila/Rendering/MaterialPreview.h"
#include "Aquila/UI/Core/TextureCache.h"
#include "Core/AssetTypes.h"
#include "Aquila/UI/Widgets/AssetSlot.h"

namespace Editor {

using Aquila::Platform::Filesystem::VirtualFileSystem;

namespace {

constexpr Uint32 K_MATERIAL_SIZE = 128;
constexpr Usize K_RENDERS_PER_FRAME = 2;
constexpr F64 K_EDIT_DELAY = 0.3;
constexpr Uint64 K_MAX_IMAGE_BYTES = 64 * 1024 * 1024;
constexpr Uint32 K_IMAGE_SIZE = 256;

bool is_vector(std::string_view path) {
	return Aquila::Foundation::ends_with_ignore_case(path, ".svg");
}

bool uses_texture(const Aquila::Graphics::MaterialData &data, const std::string &texture_path) {
	return std::ranges::any_of(data.values, [&texture_path](const auto &entry) {
		const auto *reference = std::get_if<Aquila::Graphics::TextureReference>(&entry.second);
		return reference != nullptr && reference->path == texture_path;
	});
}

}

AssetThumbnails::AssetThumbnails(Aquila::GFX::GfxContext &gfx, Aquila::UI::Core::TextureCache &textures,
								 const AssetTypes &types)
	: m_gfx(gfx), m_textures(textures), m_types(types),
	  m_preview(std::make_unique<Aquila::Rendering::MaterialPreview>(gfx, K_MATERIAL_SIZE)) {
	m_material_connection =
		Aquila::Graphics::MaterialAssets::get()->on_changed.connect([this](const std::string &path) {
			if (m_materials.contains(path)) {
				queue(path, K_EDIT_DELAY);
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
	const AssetType *type = m_types.find_for_path(path);
	const AssetThumbnail kind = type != nullptr ? type->thumbnail : AssetThumbnail::None;
	if (kind == AssetThumbnail::Image) {
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
	if (kind != AssetThumbnail::Material) {
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
	if (m_image_requests.contains(path) || vfs->get_file_size(path) > K_MAX_IMAGE_BYTES) {
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
			image.mips = Aquila::Graphics::Texture::build_mip_chain(raw.pixels.get(), raw.width, raw.height,
																	Aquila::Graphics::Texture::MipFilter::AlphaWeighted,
																	K_IMAGE_SIZE);
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
		if (image.generation != m_generation || image.mips.levels.empty()) {
			continue;
		}
		Ref<Aquila::GFX::GfxTexture> texture = Aquila::Graphics::Texture::upload_mip_chain(
			m_gfx, image.mips, Aquila::RHI::TextureFormat::RGBA8,
			Aquila::RHI::SamplerDesc::ui_image(static_cast<F32>(image.mips.levels.size())),
			image.path + " (thumbnail)");
		if (!texture) {
			continue;
		}
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
	for (auto it = m_queue.begin(); it != m_queue.end() && due.size() < K_RENDERS_PER_FRAME;) {
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
			queue(material_path, K_EDIT_DELAY);
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
