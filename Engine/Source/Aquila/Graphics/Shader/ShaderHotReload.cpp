#include "Aquila/Graphics/Shader/ShaderHotReload.h"
#include "Aquila/Foundation/FrameScheduler.h"

namespace Aquila::Graphics::Shader {

Uint64 ShaderHotReload::register_reloadable(const std::string &shader_path, ReloadCallback on_change) {
	Uint64 id = m_next_id++;
	m_entries[id] = { shader_path, std::move(on_change) };
	m_watcher.watch_slang_file(shader_path, shader_path);
	return id;
}

void ShaderHotReload::unregister(Uint64 id) {
	auto it = m_entries.find(id);
	if (it == m_entries.end()) {
		return;
	}

	std::string path = it->second.path;
	m_entries.erase(it);

	bool still_watched = false;
	for (const auto &[other_id, entry] : m_entries) {
		if (entry.path == path) {
			still_watched = true;
			break;
		}
	}
	if (!still_watched) {
		m_watcher.unwatch(path);
	}
}

void ShaderHotReload::tick() {
	auto changed = m_watcher.check_for_changes();
	if (changed.empty()) {
		return;
	}

	std::vector<ReloadCallback> to_run;
	for (const auto &[id, entry] : m_entries) {
		if (changed.count(entry.path) != 0) {
			to_run.push_back(entry.callback);
		}
	}

	for (auto &callback : to_run) {
		callback();
	}

	Foundation::FrameScheduler::get()->request_frame();
}

} // namespace Aquila::Graphics::Shader
