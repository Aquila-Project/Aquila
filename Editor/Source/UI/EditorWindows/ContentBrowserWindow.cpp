#include "UI/EditorWindows/ContentBrowserWindow.h"

#include "Aquila/Foundation/FrameScheduler.h"
#include "Aquila/Foundation/Macros.h"
#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"
#include "Aquila/UI/Widgets/AssetPayload.h"
#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/Image.h"
#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/TextInput.h"

#include <algorithm>
#include <cctype>
#include <initializer_list>

namespace Editor {

using namespace Aquila::UI::Core;
using Aquila::Platform::Filesystem::VirtualFileSystem;

namespace {

constexpr std::string_view k_root = "/resources";
constexpr Usize k_max_name_length = 14;
constexpr std::chrono::milliseconds k_double_click{ 400 };

std::string lower(std::string_view text) {
	std::string out(text);
	std::ranges::transform(out, out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return out;
}

std::string asset_type(std::string_view name) {
	const std::string file = lower(name);
	auto has_extension = [&file](std::initializer_list<std::string_view> extensions) {
		return std::ranges::any_of(extensions,
								   [&file](std::string_view extension) { return file.ends_with(extension); });
	};
	if (file.ends_with(".d.luau")) {
		return "definitions";
	}
	if (has_extension({ ".luau", ".lua" })) {
		return "script";
	}
	if (has_extension({ ".png", ".jpg", ".jpeg", ".hdr", ".tga", ".svg" })) {
		return "texture";
	}
	if (has_extension({ ".gltf", ".glb", ".obj", ".fbx" })) {
		return "model";
	}
	if (has_extension({ ".slang", ".hlsl", ".glsl" })) {
		return "shader";
	}
	if (has_extension({ ".aqlayout", ".aqstyle" })) {
		return "ui";
	}
	return "file";
}

std::string_view icon_for(std::string_view type) {
	if (type == "folder") {
		return "folder";
	}
	if (type == "script" || type == "definitions" || type == "shader") {
		return "file-code";
	}
	if (type == "texture") {
		return "image";
	}
	if (type == "model") {
		return "box";
	}
	if (type == "ui") {
		return "panel-left";
	}
	return "file";
}

std::string shorten(const std::string &name) {
	return name.size() <= k_max_name_length ? name : name.substr(0, k_max_name_length - 3) + "...";
}

class ContentTile final : public View {
  public:
	ContentTile(AssetPayload payload, bool is_directory, Aquila::GFX::GfxTexture *icon)
		: m_payload(std::move(payload)) {
		m_is_draggable = !is_directory;
		set_input_leaf(true);
		set_tooltip(m_payload.display_name);
		add_class("content-tile");

		auto *image = add_child<Image>(icon);
		image->add_class("content-tile-icon");
		image->set_class("content-tile-folder", is_directory);
		add_child<Label>(shorten(m_payload.display_name))->add_class("content-tile-name");
	}

	[[nodiscard]] std::string_view get_type_name() const override { return "ContentTile"; }

	void on_drag_start(DragState &state) override {
		state.payload = m_payload;
		state.label = m_payload.display_name;
	}

  private:
	AssetPayload m_payload;
};

void clear_children(View &view) {
	while (!view.get_children().empty()) {
		view.remove_child(view.get_children().back().get());
	}
}

} // namespace

ContentBrowserWindow::ContentBrowserWindow(EditorContext &context) : EditorWindow(context), m_directory(k_root) {}

void ContentBrowserWindow::build(View &content) {
	auto layout = load_layout("content_browser.aqlayout");
	if (!layout) {
		return;
	}
	View *root = content.add_child(std::move(layout));
	m_path = root->find_by_id("content-path");
	m_grid = root->find_by_id("content-grid");
	m_filter = root->find_by_id<TextInput>("content-filter");
	if (m_path == nullptr || m_grid == nullptr || m_filter == nullptr) {
		AQUILA_LOG_ERROR("ContentBrowserWindow: content_browser.aqlayout is missing an element");
		return;
	}

	m_filter->set_placeholder("Filter");
	m_filter->on_changed.connect([this](const std::string &) { apply_filter(); });
	if (auto *up = root->find_by_id<Button>("content-up")) {
		up->on_click.connect([this] {
			if (m_directory != k_root) {
				navigate(m_directory.substr(0, m_directory.find_last_of('/')));
			}
		});
	}
	if (auto *refresh = root->find_by_id<Button>("content-refresh")) {
		refresh->on_click.connect([this] { navigate(m_directory); });
	}

	open(m_directory);
}

void ContentBrowserWindow::update(F32) {
	if (!m_pending) {
		return;
	}
	const std::string directory = std::move(*m_pending);
	m_pending.reset();
	open(directory);
}

void ContentBrowserWindow::navigate(std::string directory) {
	m_pending = std::move(directory);
	Aquila::Foundation::FrameScheduler::get()->request_frame();
}

void ContentBrowserWindow::open(const std::string &directory) {
	m_directory = VirtualFileSystem::get()->is_directory(directory) ? directory : std::string(k_root);
	build_path();
	build_grid();
}

void ContentBrowserWindow::build_path() {
	clear_children(*m_path);

	Usize start = 1;
	while (start <= m_directory.size()) {
		const Usize end = std::min(m_directory.find('/', start), m_directory.size());
		const std::string path = m_directory.substr(0, end);
		if (start > 1) {
			m_path->add_child<Image>(context().icon("chevron-right"))->add_class("content-crumb-separator");
		}

		auto *crumb = m_path->add_child<Button>(start == 1 ? "Resources" : m_directory.substr(start, end - start));
		crumb->add_class("content-crumb");
		crumb->set_class("content-crumb-current", end == m_directory.size());
		crumb->on_click.connect([this, path] { navigate(path); });
		start = end + 1;
	}
}

void ContentBrowserWindow::build_grid() {
	clear_children(*m_grid);

	auto *vfs = VirtualFileSystem::get();
	std::vector<Tile> tiles;
	for (std::string &name : vfs->list_directory(m_directory)) {
		if (name.starts_with('.')) {
			continue;
		}
		std::string path = m_directory + "/" + name;
		const bool is_directory = vfs->is_directory(path);
		tiles.push_back({ .name = std::move(name), .path = std::move(path), .is_directory = is_directory });
	}
	std::ranges::sort(tiles, [](const Tile &a, const Tile &b) {
		if (a.is_directory != b.is_directory) {
			return a.is_directory;
		}
		return lower(a.name) < lower(b.name);
	});

	for (Tile &tile : tiles) {
		const std::string type = tile.is_directory ? "folder" : asset_type(tile.name);
		tile.view = m_grid->add_child<ContentTile>(AssetPayload{ tile.path, type, tile.name }, tile.is_directory,
												   context().icon(icon_for(type)));
		tile.view->set_class("content-tile-selected", tile.path == m_selected);
		tile.view->on_pressed.connect([this, path = tile.path](Vec2) { press(path); });
	}
	m_tiles = std::move(tiles);

	if (m_tiles.empty()) {
		m_grid->add_child<Label>(std::string("This folder is empty"))->add_class("content-empty");
	}
	apply_filter();
}

void ContentBrowserWindow::apply_filter() {
	const std::string query = lower(m_filter->get_text());
	for (const Tile &tile : m_tiles) {
		tile.view->set_hidden(!query.empty() && lower(tile.name).find(query) == std::string::npos);
	}
}

void ContentBrowserWindow::press(const std::string &path) {
	const auto now = std::chrono::steady_clock::now();
	const bool double_click = path == m_selected && now - m_last_press < k_double_click;
	m_last_press = now;
	m_selected = path;

	for (const Tile &tile : m_tiles) {
		tile.view->set_class("content-tile-selected", tile.path == path);
		if (double_click && tile.path == path && tile.is_directory) {
			navigate(path);
		}
	}
}

} // namespace Editor
