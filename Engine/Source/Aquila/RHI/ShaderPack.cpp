#include "Aquila/RHI/ShaderPack.h"
#include "Aquila/Foundation/EnginePaths.h"
#include "Aquila/Foundation/Macros.h"

namespace Aquila::RHI {

namespace {

constexpr Uint32 K_MAGIC = 0x50535141;
constexpr Uint32 K_VERSION = 1;

class Writer {
  public:
	void u32(Uint32 value) {
		const auto *bytes = reinterpret_cast<const char *>(&value);
		m_data.insert(m_data.end(), bytes, bytes + sizeof(value));
	}

	void bytes(const void *data, Usize size) {
		const auto *begin = static_cast<const char *>(data);
		m_data.insert(m_data.end(), begin, begin + size);
	}

	void string(std::string_view text) {
		u32(static_cast<Uint32>(text.size()));
		bytes(text.data(), text.size());
	}

	[[nodiscard]] const std::vector<char> &data() const { return m_data; }

  private:
	std::vector<char> m_data;
};

class Reader {
  public:
	explicit Reader(std::span<const char> data) : m_data(data) {}

	bool u32(Uint32 &out) { return bytes(&out, sizeof(out)); }

	bool bytes(void *out, Usize size) {
		if (size > m_data.size() - m_offset) {
			return false;
		}
		std::memcpy(out, m_data.data() + m_offset, size);
		m_offset += size;
		return true;
	}

	bool string(std::string &out) {
		Uint32 size = 0;
		if (!u32(size) || size > m_data.size() - m_offset) {
			return false;
		}
		out.assign(m_data.data() + m_offset, size);
		m_offset += size;
		return true;
	}

	[[nodiscard]] Usize remaining() const { return m_data.size() - m_offset; }

  private:
	std::span<const char> m_data;
	Usize m_offset = 0;
};

struct Packs {
	ShaderPack engine;
	std::string engine_dir;
	bool engine_read = false;
	std::vector<ShaderPack> mounted;
};

Packs &packs() {
	static Packs instance;
	return instance;
}

void refresh_engine_pack() {
	Packs &state = packs();
	const std::string &dir = Foundation::EnginePaths::shaders_dir();
	if (state.engine_read && state.engine_dir == dir) {
		return;
	}
	state.engine_read = true;
	state.engine_dir = dir;
	state.engine = {};

	const std::string path = dir + std::string(ENGINE_SHADER_PACK);
	std::error_code ec;
	if (!std::filesystem::is_regular_file(path, ec)) {
		return;
	}
	std::string error;
	if (!state.engine.load(path, error)) {
		AQUILA_LOG_ERROR("ShaderPacks: {}", error);
		return;
	}
	AQUILA_LOG_INFO("ShaderPacks: {} precompiled engine shaders", state.engine.size());
}

}

void ShaderPack::add(std::string key, ShaderPackEntry entry) {
	m_entries.insert_or_assign(std::move(key), std::move(entry));
}

const ShaderPackEntry *ShaderPack::find(std::string_view key) const {
	const auto it = m_entries.find(key);
	return it == m_entries.end() ? nullptr : &it->second;
}

bool ShaderPack::save(const std::string &path, std::string &error) const {
	Writer writer;
	writer.u32(K_MAGIC);
	writer.u32(K_VERSION);
	writer.u32(static_cast<Uint32>(m_entries.size()));
	for (const auto &[key, entry] : m_entries) {
		writer.string(key);
		writer.u32(static_cast<Uint32>(entry.stages.size()));
		for (const ShaderPackStage &stage : entry.stages) {
			writer.u32(stage.stage);
			writer.u32(static_cast<Uint32>(stage.spirv.size()));
			writer.bytes(stage.spirv.data(), stage.spirv.size() * sizeof(Uint32));
		}
		writer.u32(static_cast<Uint32>(entry.metadata.size()));
		writer.bytes(entry.metadata.data(), entry.metadata.size());
	}

	std::error_code ec;
	std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	file.write(writer.data().data(), static_cast<std::streamsize>(writer.data().size()));
	if (!file) {
		error = "Can't write the shader pack '" + path + "'";
		return false;
	}
	return true;
}

bool ShaderPack::load(const std::string &path, std::string &error) {
	m_entries.clear();
	std::ifstream file(path, std::ios::binary);
	if (!file) {
		error = "Can't open the shader pack '" + path + "'";
		return false;
	}
	const std::vector<char> data{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };

	Reader reader(data);
	Uint32 magic = 0;
	Uint32 version = 0;
	Uint32 count = 0;
	if (!reader.u32(magic) || magic != K_MAGIC || !reader.u32(version) || version != K_VERSION || !reader.u32(count)) {
		error = "'" + path + "' is not a shader pack this build can read";
		return false;
	}

	for (Uint32 i = 0; i < count; ++i) {
		std::string key;
		ShaderPackEntry entry;
		Uint32 stage_count = 0;
		bool ok = reader.string(key) && reader.u32(stage_count);
		for (Uint32 s = 0; ok && s < stage_count; ++s) {
			ShaderPackStage stage;
			Uint32 words = 0;
			ok = reader.u32(stage.stage) && reader.u32(words) && words <= reader.remaining() / sizeof(Uint32);
			if (ok) {
				stage.spirv.resize(words);
				ok = reader.bytes(stage.spirv.data(), words * sizeof(Uint32));
			}
			entry.stages.push_back(std::move(stage));
		}
		Uint32 metadata_size = 0;
		ok = ok && reader.u32(metadata_size) && metadata_size <= reader.remaining();
		if (ok) {
			entry.metadata.resize(metadata_size);
			ok = reader.bytes(entry.metadata.data(), metadata_size);
		}
		if (!ok) {
			m_entries.clear();
			error = "The shader pack '" + path + "' is truncated";
			return false;
		}
		m_entries.insert_or_assign(std::move(key), std::move(entry));
	}
	return true;
}

std::string shader_key(std::string_view path, std::string_view variant) {
	const std::string &engine_dir = Foundation::EnginePaths::shaders_dir();
	if (!engine_dir.empty() && path.starts_with(engine_dir)) {
		path.remove_prefix(engine_dir.size());
	}
	std::string key(path);
	std::ranges::replace(key, '\\', '/');
	if (!variant.empty()) {
		key += ':';
		key += variant;
	}
	return key;
}

namespace ShaderPacks {

bool mount(const std::string &path, std::string &error) {
	ShaderPack pack;
	if (!pack.load(path, error)) {
		return false;
	}
	AQUILA_LOG_INFO("ShaderPacks: {} precompiled shaders from '{}'", pack.size(), path);
	packs().mounted.push_back(std::move(pack));
	return true;
}

const ShaderPackEntry *find(std::string_view key) {
	refresh_engine_pack();
	const Packs &state = packs();
	for (auto it = state.mounted.rbegin(); it != state.mounted.rend(); ++it) {
		if (const ShaderPackEntry *entry = it->find(key)) {
			return entry;
		}
	}
	return state.engine.find(key);
}

void clear() {
	packs() = {};
}

}

}
