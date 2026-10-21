#ifndef AQUILA_RHI_SHADER_PACK_H
#define AQUILA_RHI_SHADER_PACK_H

#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Aquila::RHI {

inline constexpr std::string_view ENGINE_SHADER_PACK = "Engine.aqshaders";
inline constexpr std::string_view PROJECT_SHADER_PACK = "Shaders.aqshaders";

struct ShaderPackStage {
	Uint32 stage = 0;
	std::vector<Uint32> spirv;
};

struct ShaderPackEntry {
	std::vector<ShaderPackStage> stages;
	std::vector<Uint8> metadata;
};

class ShaderPack {
  public:
	void add(std::string key, ShaderPackEntry entry);
	[[nodiscard]] const ShaderPackEntry *find(std::string_view key) const;
	[[nodiscard]] Usize size() const { return m_entries.size(); }

	bool save(const std::string &path, std::string &error) const;
	bool load(const std::string &path, std::string &error);

  private:
	std::map<std::string, ShaderPackEntry, std::less<>> m_entries;
};

[[nodiscard]] std::string shader_key(std::string_view path, std::string_view variant = {});

namespace ShaderPacks {

bool mount(const std::string &path, std::string &error);
[[nodiscard]] const ShaderPackEntry *find(std::string_view key);
void clear();

}

}

#endif
