#include "Aquila/Graphics/Texture/TextureLibrary.h"
#include "Aquila/Foundation/FrameScheduler.h"
#include "Aquila/Foundation/Macros.h"
#include "Aquila/GFX/GfxContext.h"
#include "Aquila/Graphics/Texture/TextureLoader.h"
#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"

namespace Aquila::Graphics {

namespace {

const std::array<F32, 256> &srgb_to_linear_table() {
	static const std::array<F32, 256> table = [] {
		std::array<F32, 256> out{};
		for (Usize i = 0; i < out.size(); ++i) {
			const F32 c = static_cast<F32>(i) / 255.F;
			out[i] = c <= 0.04045F ? c / 12.92F : std::pow((c + 0.055F) / 1.055F, 2.4F);
		}
		return out;
	}();
	return table;
}

Uint8 linear_to_srgb_byte(F32 linear) {
	const F32 c = std::clamp(linear, 0.F, 1.F);
	const F32 encoded = c <= 0.0031308F ? c * 12.92F : 1.055F * std::pow(c, 1.F / 2.4F) - 0.055F;
	return static_cast<Uint8>(std::lround(encoded * 255.F));
}

std::vector<std::vector<Uint8>> build_mips(const Uint8 *pixels, Uint32 width, Uint32 height, bool srgb) {
	std::vector<std::vector<Uint8>> levels;
	levels.emplace_back(pixels, pixels + static_cast<Usize>(width) * height * 4);
	const auto &to_linear = srgb_to_linear_table();

	Uint32 w = width;
	Uint32 h = height;
	while (w > 1 || h > 1) {
		const Uint32 next_w = std::max(1U, w / 2);
		const Uint32 next_h = std::max(1U, h / 2);
		const std::vector<Uint8> &src = levels.back();
		std::vector<Uint8> dst(static_cast<Usize>(next_w) * next_h * 4);
		for (Uint32 y = 0; y < next_h; ++y) {
			for (Uint32 x = 0; x < next_w; ++x) {
				for (Uint32 c = 0; c < 4; ++c) {
					F32 sum = 0.F;
					for (Uint32 dy = 0; dy < 2; ++dy) {
						for (Uint32 dx = 0; dx < 2; ++dx) {
							const Uint32 sx = std::min(x * 2 + dx, w - 1);
							const Uint32 sy = std::min(y * 2 + dy, h - 1);
							const Uint8 value = src[(static_cast<Usize>(sy) * w + sx) * 4 + c];
							sum += srgb && c < 3 ? to_linear[value] : static_cast<F32>(value) / 255.F;
						}
					}
					const F32 average = sum * 0.25F;
					dst[(static_cast<Usize>(y) * next_w + x) * 4 + c] = srgb && c < 3
						? linear_to_srgb_byte(average)
						: static_cast<Uint8>(std::lround(std::clamp(average, 0.F, 1.F) * 255.F));
				}
			}
		}
		levels.push_back(std::move(dst));
		w = next_w;
		h = next_h;
	}
	return levels;
}

constexpr Usize k_upload_budget_bytes = 64ULL * 1024ULL * 1024ULL;

}

struct TextureLibrary::Decoded {
	Key key;
	Uint64 generation = 0;
	Uint64 write_time = 0;
	Uint32 width = 0;
	Uint32 height = 0;
	std::vector<std::vector<Uint8>> mips;
	Vec4 average = Vec4(1.F);

	[[nodiscard]] bool is_valid() const { return !mips.empty(); }
	[[nodiscard]] Usize size_bytes() const {
		Usize total = 0;
		for (const std::vector<Uint8> &level : mips) {
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

	decoded.width = image.width;
	decoded.height = image.height;
	decoded.mips = build_mips(image.pixels.get(), image.width, image.height, srgb);
	const std::vector<Uint8> &smallest = decoded.mips.back();
	const auto &to_linear = srgb_to_linear_table();
	for (Uint32 c = 0; c < 4; ++c) {
		decoded.average[static_cast<int>(c)] =
			srgb && c < 3 ? to_linear[smallest[c]] : static_cast<F32>(smallest[c]) / 255.F;
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

Ref<GFX::GfxTexture> TextureLibrary::upload(GFX::GfxContext &ctx, const std::string &path, Decoded &decoded) const {
	RHI::TextureDesc desc{};
	desc.width = decoded.width;
	desc.height = decoded.height;
	desc.format = decoded.key.second ? RHI::TextureFormat::RgbA8Srgb : RHI::TextureFormat::RGBA8;
	desc.usage = RHI::TextureUsage::Sampled | RHI::TextureUsage::TransferDst;
	desc.mip_levels = static_cast<Uint32>(decoded.mips.size());
	desc.sampler = RHI::SamplerDesc::texture2_d(static_cast<F32>(decoded.mips.size()));
	desc.debug_name = path;
	Ref<GFX::GfxTexture> texture = ctx.create_texture(desc);
	if (texture) {
		ctx.upload_texture_mips(*texture, decoded.mips);
	}
	return texture;
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
		while (split != m_inbox->done.end() && (budget == 0 || budget + split->size_bytes() <= k_upload_budget_bytes)) {
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
		Ref<GFX::GfxTexture> texture = decoded.is_valid() ? upload(ctx, key.first, decoded) : nullptr;
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
		RHI::TextureDesc desc{};
		desc.format = RHI::TextureFormat::RGBA8;
		desc.usage = RHI::TextureUsage::Sampled | RHI::TextureUsage::TransferDst;
		desc.debug_name = "DefaultMaterialTexture";
		slot = ctx.create_texture(desc);
		ctx.upload_texture_mips(*slot, { std::vector<Uint8>(pixel.begin(), pixel.end()) });
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
