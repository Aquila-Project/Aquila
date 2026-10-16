#pragma once
#include "Aquila/Foundation/Singleton.h"
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Graphics/Shader/ShaderWatcher.h"

namespace Aquila::Graphics::Shader {

class ShaderHotReload : public Foundation::Singleton<ShaderHotReload> {
  public:
	using ReloadCallback = Delegate<void()>;

	ShaderHotReload() { s_alive = true; }
	~ShaderHotReload() { s_alive = false; }

	Uint64 register_reloadable(const std::string &shader_path, ReloadCallback on_change);
	void unregister(Uint64 id);

	void tick();

	void enable(bool enable) { m_watcher.enable(enable); }
	[[nodiscard]] bool is_enabled() const { return m_watcher.is_enabled(); }

	[[nodiscard]] static bool is_alive() { return s_alive; }

  private:
	struct Entry {
		std::string path;
		ReloadCallback callback;
	};

	ShaderWatcher m_watcher;
	std::unordered_map<Uint64, Entry> m_entries;
	Uint64 m_next_id = 1;

	static inline bool s_alive = false;
};

} // namespace Aquila::Graphics::Shader
