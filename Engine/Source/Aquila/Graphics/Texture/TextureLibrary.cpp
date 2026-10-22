#include "Aquila/Graphics/Texture/TextureLibrary.h"
#include "Aquila/Foundation/FrameScheduler.h"
#include "Aquila/Foundation/Macros.h"
#include "Aquila/GFX/GfxContext.h"
#include "Aquila/Graphics/Texture/MipChain.h"
#include "Aquila/Graphics/Texture/TextureLoader.h"
#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"

namespace Aquila::Graphics {

namespace {

constexpr Usize K_UPLOAD_BUDGET_BYTES = 64ULL * 1024ULL * 1024ULL;

}

struct TextureLibrary::Decoded {
	Key key;
	Uint64 generation = 0;
	Uint64 write_time = 0;
	Texture::MipChain image;
	Vec4 average = Vec4(1.F);

	[[nodiscard]] bool is_valid() const { return !image.levels.empty(); }
	[[nodiscard]] Usize size_bytes() const {
		Usize total = 0;
		for (const std::vector<Uint8> &level : image.levels) {
			total += level.size();
		}
		return total;
	}
};

struct TextureLibrary::Inbox {
	std::mutex mutex;
	std::vector<Decoded> done;
};

TextureLibrary::TextureLibrary() : m_inbox(std::make_shared<Inbox>()) {}

TextureLibrary::~TextureLibrary() = default;

Ref<GFX::GfxTexture> TextureLibrary::load(GFX::GfxContext &, const std::string &path, bool srgb) {
	if (path.empty()) {
		return nullptr;
	}
	Key key = std::make_pair(path, srgb);
	if (auto it = m_textures.find(key); it != m_textures.end()) {
		return it->second.texture;
	}
	if (!m_failed.contains(key) && !m_loading.contains(key)) {
		start(key, Foundation::JobPriority::High);
	}
	return nullptr;
}

void TextureLibrary::preload(const std::string &path, bool srgb, Foundation::JobPriority priority) {
	if (path.empty()) {
		return;
	}
	Key key = std::make_pair(path, srgb);
	if (!m_textures.contains(key) && !m_failed.contains(key) && !m_loading.contains(key)) {
		start(key, priority);
	}
}

bool TextureLibrary::is_loading(const std::string &path, bool srgb) const {
	return m_loading.contains(std::make_pair(path, srgb));
}

TextureLibrary::Decoded TextureLibrary::decode(const std::string &path, const Option<std::string> &native_path,
												bool srgb) {
	Decoded decoded;
	Texture::TextureLoader::RawImageData image;
	try {
		image = native_path ? Texture::TextureLoader().load_from_native(*native_path)
							: Texture::TextureLoader().load_from_vfs(path);
	} catch (const std::exception &error) {
		AQUILA_LOG_WARNING("TextureLibrary: {}", error.what());
	}
	if (!image.is_valid()) {
		AQUILA_LOG_WARNING("TextureLibrary: can't decode '{}'", path);
		return decoded;
	}

	decoded.image = Texture::build_mip_chain(image.pixels.get(), image.width, image.height,
											 srgb ? Texture::MipFilter::BoxSrgb : Texture::MipFilter::Box);
	const std::vector<Uint8> &smallest = decoded.image.levels.back();
	for (Uint32 c = 0; c < 4; ++c) {
		decoded.average[static_cast<int>(c)] =
			srgb && c < 3 ? Texture::srgb_to_linear(smallest[c]) : static_cast<F32>(smallest[c]) / 255.F;
	}
	return decoded;
}

void TextureLibrary::start(const Key &key, Foundation::JobPriority priority) {
	m_loading.insert(key);
	auto *vfs = Platform::Filesystem::VirtualFileSystem::get();
	auto job = [inbox = m_inbox, key, generation = m_generation, native_path = vfs->to_native_path(key.first),
				write_time = vfs->get_last_write_time(key.first)] {
		Decoded decoded = decode(key.first, native_path, key.second);
		decoded.key = key;
		decoded.generation = generation;
		decoded.write_time = write_time;
		{
			std::scoped_lock lock(inbox->mutex);
			inbox->done.push_back(std::move(decoded));
		}
		Foundation::FrameScheduler::get()->request_frame();
	};
	Foundation::JobSystem::get()->schedule(priority, "Load texture " + key.first, std::move(job));
}

bool TextureLibrary::update(GFX::GfxContext &ctx) {
	std::vector<Decoded> ready;
	{
		std::scoped_lock lock(m_inbox->mutex);
		if (m_inbox->done.empty()) {
			return false;
		}
		Usize budget = 0;
		auto split = m_inbox->done.begin();
		while (split != m_inbox->done.end() && (budget == 0 || budget + split->size_bytes() <= K_UPLOAD_BUDGET_BYTES)) {
			budget += split->size_bytes();
			++split;
		}
		ready.assign(std::make_move_iterator(m_inbox->done.begin()), std::make_move_iterator(split));
		m_inbox->done.erase(m_inbox->done.begin(), split);
		if (!m_inbox->done.empty()) {
			Foundation::FrameScheduler::get()->request_frame();
		}
	}

	std::vector<std::string> loaded;
	std::vector<std::string> reloaded;
	bool failed = false;
	for (Decoded &decoded : ready) {
		if (decoded.generation != m_generation) {
			continue;
		}
		const Key &key = decoded.key;
		m_loading.erase(key);
		Ref<GFX::GfxTexture> texture = decoded.is_valid()
			? Texture::upload_mip_chain(
				  ctx, decoded.image, key.second ? RHI::TextureFormat::RgbA8Srgb : RHI::TextureFormat::RGBA8,
				  RHI::SamplerDesc::texture2_d(static_cast<F32>(decoded.image.levels.size())), key.first)
			: nullptr;
		auto existing = m_textures.find(key);
		if (!texture) {
			if (existing == m_textures.end()) {
				m_failed.insert_or_assign(key, decoded.write_time);
				failed = true;
			}
			continue;
		}
		if (existing != m_textures.end()) {
			existing->second =
				Entry{ .texture = std::move(texture), .write_time = decoded.write_time, .average = decoded.average };
			reloaded.push_back(key.first);
		} else {
			m_textures.emplace(
				key,
				Entry{ .texture = std::move(texture), .write_time = decoded.write_time, .average = decoded.average });
			loaded.push_back(key.first);
		}
	}
	if (loaded.empty() && reloaded.empty() && !failed) {
		return false;
	}

	++m_revision;
	for (const std::string &path : loaded) {
		on_loaded(path);
	}
	for (const std::string &path : reloaded) {
		AQUILA_LOG_INFO("TextureLibrary: reloaded '{}'", path);
		on_reloaded(path);
	}
	return !loaded.empty() || !reloaded.empty();
}

void TextureLibrary::finish_loading(GFX::GfxContext &ctx) {
	while (!m_loading.empty()) {
		Foundation::JobSystem::get()->wait_for_all();
		bool waiting = false;
		{
			std::scoped_lock lock(m_inbox->mutex);
			waiting = !m_inbox->done.empty();
		}
		if (!waiting) {
			break;
		}
		update(ctx);
	}
}

bool TextureLibrary::poll_changes(GFX::GfxContext &) {
	auto *vfs = Platform::Filesystem::VirtualFileSystem::get();
	bool retry = false;

	for (auto it = m_failed.begin(); it != m_failed.end();) {
		const Uint64 write_time = vfs->get_last_write_time(it->first.first);
		if (write_time != 0 && write_time != it->second) {
			it = m_failed.erase(it);
			retry = true;
		} else {
			++it;
		}
	}

	for (auto &[key, entry] : m_textures) {
		const Uint64 write_time = vfs->get_last_write_time(key.first);
		if (write_time == 0 || write_time == entry.write_time || m_loading.contains(key)) {
			continue;
		}
		entry.write_time = write_time;
		start(key, Foundation::JobPriority::High);
	}

	if (retry) {
		++m_revision;
	}
	return retry;
}

Option<Vec4> TextureLibrary::get_average(const std::string &path, bool srgb) const {
	const auto it = m_textures.find(std::make_pair(path, srgb));
	if (it == m_textures.end()) {
		return std::nullopt;
	}
	return it->second.average;
}

GFX::GfxTexture &TextureLibrary::get_default(GFX::GfxContext &ctx, DefaultTexture kind) {
	Ref<GFX::GfxTexture> &slot = m_defaults[static_cast<Usize>(kind)];
	if (!slot) {
		std::array<Uint8, 4> pixel{ 255, 255, 255, 255 };
		if (kind == DefaultTexture::Black) {
			pixel = { 0, 0, 0, 255 };
		} else if (kind == DefaultTexture::FlatNormal) {
			pixel = { 128, 128, 255, 255 };
		} else if (kind == DefaultTexture::Gray) {
			pixel = { 128, 128, 128, 255 };
		}
		const Texture::MipChain chain{ .width = 1, .height = 1, .levels = { { pixel.begin(), pixel.end() } } };
		slot = Texture::upload_mip_chain(ctx, chain, RHI::TextureFormat::RGBA8, RHI::SamplerDesc::texture2_d(),
										 "DefaultMaterialTexture");
	}
	return *slot;
}

void TextureLibrary::clear() {
	m_textures.clear();
	m_failed.clear();
	m_loading.clear();
	++m_generation;
	std::scoped_lock lock(m_inbox->mutex);
	m_inbox->done.clear();
}

}
