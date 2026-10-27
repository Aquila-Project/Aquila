#include "Aquila/UI/Style/StyleSet.h"

#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"

namespace Aquila::UI {

namespace {

constexpr std::string_view K_LAYOUT_EXTENSION = ".aqlayout";
constexpr std::string_view K_STYLE_EXTENSION = ".aqstyle";

std::string paired_style(const std::string &layout_path) {
	std::string style = layout_path;
	if (style.ends_with(K_LAYOUT_EXTENSION)) {
		style.resize(style.size() - K_LAYOUT_EXTENSION.size());
	}
	return style + std::string(K_STYLE_EXTENSION);
}

}

void StyleSet::set_base(std::vector<std::string> paths) {
	if (paths != m_base) {
		m_base = std::move(paths);
		++m_revision;
	}
}

void StyleSet::note_layout(const std::string &layout_path, bool has_root, std::vector<std::string> imports) {
	const std::vector<std::string> before = get_paths();
	std::string style = paired_style(layout_path);
	if (!Platform::Filesystem::VirtualFileSystem::get()->exists(style)) {
		style.clear();
	}
	m_layouts[layout_path] = { .has_root = has_root, .imports = std::move(imports), .style = std::move(style) };
	if (get_paths() != before) {
		++m_revision;
	}
}

std::vector<std::string> StyleSet::get_paths() const {
	std::vector<std::string> paths = m_base;
	std::set<std::string> visited;
	for (const auto &[path, layout] : m_layouts) {
		if (!layout.has_root) {
			add_component(path, visited, paths);
		}
	}
	for (const auto &[path, layout] : m_layouts) {
		if (layout.has_root && !layout.style.empty() && std::ranges::find(paths, layout.style) == paths.end()) {
			paths.push_back(layout.style);
		}
	}
	return paths;
}

void StyleSet::add_component(const std::string &path, std::set<std::string> &visited,
							 std::vector<std::string> &out) const {
	if (!visited.insert(path).second) {
		return;
	}
	const auto it = m_layouts.find(path);
	if (it == m_layouts.end() || it->second.has_root) {
		return;
	}
	for (const std::string &import : it->second.imports) {
		add_component(import, visited, out);
	}
	if (!it->second.style.empty() && std::ranges::find(out, it->second.style) == out.end()) {
		out.push_back(it->second.style);
	}
}

}
