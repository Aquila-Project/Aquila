#include "UI/EditorWindows/ContentBrowserWindow.h"

#include "Aquila/Foundation/FrameScheduler.h"
#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/GFX/GfxTexture.h"
#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"
#include "Aquila/Platform/Process.h"
#include "Aquila/UI/Core/TextureCache.h"
#include "Aquila/UI/Widgets/AssetPayload.h"
#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/Image.h"
#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/PopupMenu.h"
#include "Aquila/UI/Widgets/TextInput.h"
#include "Core/FileDialog.h"

#include <initializer_list>

namespace Editor {

using namespace Aquila::UI::Core;
using Aquila::Platform::KeyCode;
using Aquila::Platform::Filesystem::VirtualFileSystem;
using Aquila::UI::StyleLength;
using Aquila::UI::StyleProperties;

namespace {

constexpr std::string_view k_root = Aquila::SharedConstants::PROJECT_CONTENT_DIR;
constexpr Usize k_line_length = 12;
constexpr std::chrono::milliseconds k_double_click{ 400 };
constexpr F32 k_thumbnail_size = 48.F;
constexpr Int64 k_max_preview_bytes = 8 * 1024 * 1024;

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

bool is_previewable(const std::string &path) {
	const std::string file = lower(path);
	const bool image =
		std::ranges::any_of(std::initializer_list<std::string_view>{ ".png", ".jpg", ".jpeg", ".tga", ".bmp", ".svg" },
							[&file](std::string_view extension) { return file.ends_with(extension); });
	return image && VirtualFileSystem::get()->get_file_size(path) <= k_max_preview_bytes;
}

StyleProperties fit_thumbnail(const Aquila::GFX::GfxTexture &texture) {
	const F32 width = static_cast<F32>(std::max(1U, texture.get_width()));
	const F32 height = static_cast<F32>(std::max(1U, texture.get_height()));
	const F32 scale = k_thumbnail_size / std::max(width, height);
	StyleProperties style;
	style.width = StyleLength::pixel(width * scale);
	style.height = StyleLength::pixel(height * scale);
	return style;
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

bool is_break(std::string_view name, Usize i) {
	const char c = name[i];
	return c == '.' || c == '_' || c == '-' || c == ' ' ||
		(std::isupper(static_cast<unsigned char>(c)) != 0 &&
		 std::islower(static_cast<unsigned char>(name[i - 1])) != 0);
}

std::string wrap_name(std::string_view name) {
	std::string wrapped;
	while (name.size() > k_line_length) {
		Usize cut = k_line_length;
		for (Usize i = k_line_length; i > 0; --i) {
			if (is_break(name, i)) {
				cut = i;
				break;
			}
		}
		wrapped.append(name.substr(0, cut)).push_back('\n');
		name.remove_prefix(cut);
		if (name.starts_with(' ')) {
			name.remove_prefix(1);
		}
	}
	return wrapped.append(name);
}

class RenameField final : public TextInput {
  public:
	explicit RenameField(Delegate<void(Option<std::string>)> done) : m_done(std::move(done)) {
		add_class("content-tile-rename");
		on_submit.connect([this](const std::string &text) { finish(text); });
	}

	void on_key_press(KeyCode key, int mods) override {
		if (key == KeyCode::Escape) {
			finish(std::nullopt);
			return;
		}
		TextInput::on_key_press(key, mods);
	}

	void on_focus_lost() override {
		TextInput::on_focus_lost();
		finish(get_text());
	}

	void select_stem() {
		const Usize dot = m_state.text.find('.');
		m_state.select_anchor = 0;
		m_state.cursor = dot == 0 || dot == std::string::npos ? m_state.text.size() : dot;
		queue_redraw();
	}

  private:
	void finish(Option<std::string> name) {
		if (m_finished) {
			return;
		}
		m_finished = true;
		m_done(std::move(name));
	}

	Delegate<void(Option<std::string>)> m_done;
	bool m_finished = false;
};

class ContentTile final : public View {
  public:
	ContentTile(AssetPayload payload, bool is_directory, Aquila::GFX::GfxTexture *icon, bool thumbnail)
		: m_payload(std::move(payload)) {
		m_is_draggable = !is_directory;
		set_input_leaf(true);
		set_tooltip(m_payload.display_name);
		add_class("content-tile");

		auto *preview = add_child<View>();
		preview->add_class("content-tile-preview");
		auto *image = preview->add_child<Image>(icon);
		if (thumbnail) {
			image->add_class("content-tile-thumbnail");
			image->set_style(fit_thumbnail(*icon));
		} else {
			image->add_class("content-tile-icon");
			image->set_class("content-tile-folder", is_directory);
		}
		m_name = add_child<Label>(wrap_name(m_payload.display_name));
		m_name->add_class("content-tile-name");
	}

	Delegate<void(KeyCode)> on_key;

	void begin_rename(Delegate<void(Option<std::string>)> done) {
		set_input_leaf(false);
		m_name->set_hidden(true);
		auto *field = add_child<RenameField>(std::move(done));
		field->set_text(m_payload.display_name);
		field->request_focus();
		field->select_stem();
	}

	void on_key_press(KeyCode key, int) override {
		if (on_key) {
			on_key(key);
		}
	}

	[[nodiscard]] std::string_view get_type_name() const override { return "ContentTile"; }

	void on_drag_start(DragState &state) override {
		state.payload = m_payload;
		state.label = m_payload.display_name;
	}

  private:
	AssetPayload m_payload;
	Label *m_name = nullptr;
};

std::string unique_path(const std::string &directory, const std::string &name) {
	auto *vfs = VirtualFileSystem::get();
	const Usize dot = name.find('.');
	const std::string stem = name.substr(0, dot);
	const std::string extension = dot == std::string::npos ? std::string{} : name.substr(dot);
	std::string path = directory + "/" + name;
	for (int i = 2; vfs->exists(path) || vfs->is_directory(path); ++i) {
		path = directory + "/" + stem + std::to_string(i) + extension;
	}
	return path;
}

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

	m_menu = dynamic_cast<PopupMenu *>(overlay_root()->add_child(std::make_unique<PopupMenu>()));
	m_confirm = dynamic_cast<PopupMenu *>(overlay_root()->add_child(std::make_unique<PopupMenu>()));
	m_grid->on_context_menu.connect([this](Vec2 position) { open_background_menu(position); });
	if (auto *scroll = root->find_by_id("content-scroll")) {
		scroll->on_context_menu.connect([this](Vec2 position) { open_background_menu(position); });
	}

	open(m_directory);
}

void ContentBrowserWindow::update(F32) {
	for (Delegate<void()> &action : std::exchange(m_deferred, {})) {
		action();
	}
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

void ContentBrowserWindow::defer(Delegate<void()> action) {
	m_deferred.push_back(std::move(action));
	Aquila::Foundation::FrameScheduler::get()->request_frame();
}

void ContentBrowserWindow::open(const std::string &directory) {
	const bool inside = directory == k_root || directory.starts_with(std::string(k_root) + "/");
	m_directory = inside && VirtualFileSystem::get()->is_directory(directory) ? directory : std::string(k_root);
	build_path();
	build_grid();
	if (!m_rename_after_open.empty()) {
		begin_rename(std::exchange(m_rename_after_open, {}));
		return;
	}
	if (auto tile = std::ranges::find(m_tiles, m_selected, &Tile::path); tile != m_tiles.end()) {
		tile->view->request_focus();
	}
}

void ContentBrowserWindow::build_path() {
	clear_children(*m_path);

	const auto add_crumb = [this](std::string label, std::string path) {
		if (!m_path->get_children().empty()) {
			m_path->add_child<Image>(context().icon("chevron-right"))->add_class("content-crumb-separator");
		}
		auto *crumb = m_path->add_child<Button>(std::move(label));
		crumb->add_class("content-crumb");
		crumb->set_class("content-crumb-current", path == m_directory);
		crumb->on_click.connect([this, path = std::move(path)] { navigate(path); });
	};

	add_crumb(std::string(Aquila::SharedConstants::PROJECT_CONTENT_FOLDER), std::string(k_root));
	Usize start = k_root.size() + 1;
	while (start <= m_directory.size()) {
		const Usize end = std::min(m_directory.find('/', start), m_directory.size());
		add_crumb(m_directory.substr(start, end - start), m_directory.substr(0, end));
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
		Aquila::GFX::GfxTexture *thumbnail =
			!tile.is_directory && is_previewable(tile.path) ? context().textures().load(tile.path) : nullptr;
		tile.view = m_grid->add_child<ContentTile>(AssetPayload{ tile.path, type, tile.name }, tile.is_directory,
												   thumbnail != nullptr ? thumbnail : context().icon(icon_for(type)),
												   thumbnail != nullptr);
		tile.view->set_class("content-tile-selected", tile.path == m_selected);
		tile.view->on_pressed.connect([this, path = tile.path](Vec2) { press(path); });
		tile.view->on_context_menu.connect([this, path = tile.path](Vec2 position) { open_item_menu(path, position); });
		static_cast<ContentTile *>(tile.view)->on_key = [this, path = tile.path, view = tile.view](KeyCode key) {
			if (key == KeyCode::F2) {
				begin_rename(path);
			} else if (key == KeyCode::Delete) {
				const Rect rect = view->get_absolute_rect();
				confirm_delete(path, { rect.position.x, rect.position.y + rect.size.y });
			}
		};
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

void ContentBrowserWindow::open_background_menu(Vec2 position) {
	m_menu->clear_items();
	const auto &creators = context().content_creators;
	for (Usize i = 0; i < creators.size(); ++i) {
		m_menu->add_item("New " + creators[i].label, {}, context().icon(creators[i].icon),
						 [this, i] { create(context().content_creators[i]); });
	}
	m_menu->add_separator();
	m_menu->add_item("Import Files...", {}, context().icon("plus"), [this] { defer([this] { import_files(); }); });
	m_menu->add_item(Aquila::Platform::file_manager_action_name(), {}, context().icon("folder-open"),
					 [this] { Aquila::Platform::show_in_file_manager(context().to_native(m_directory)); });
	m_menu->add_item("Refresh", {}, context().icon("refresh-cw"), [this] { navigate(m_directory); });
	m_menu->open_at(position);
}

void ContentBrowserWindow::import_files() {
	const std::vector<std::string> files = FileDialog::open_files({}, {});
	if (files.empty()) {
		return;
	}
	std::string last;
	for (const std::string &file : files) {
		const std::string target = unique_path(m_directory, std::filesystem::path(file).filename().string());
		std::error_code error;
		std::filesystem::copy(file, context().to_native(target), std::filesystem::copy_options::recursive, error);
		if (error) {
			AQUILA_LOG_WARNING("ContentBrowserWindow: couldn't import {}: {}", file, error.message());
			continue;
		}
		last = target;
	}
	if (!last.empty()) {
		m_selected = last;
	}
	navigate(m_directory);
}

void ContentBrowserWindow::create(const ContentCreator &creator) {
	const std::string path = unique_path(m_directory, creator.name);
	if (!creator.create(path)) {
		AQUILA_LOG_WARNING("ContentBrowserWindow: could not create {}", path);
		return;
	}
	m_selected = path;
	m_rename_after_open = path;
	navigate(m_directory);
}

void ContentBrowserWindow::open_item_menu(const std::string &path, Vec2 position) {
	press(path);
	m_menu->clear_items();
	m_menu->add_item("Rename", "F2", context().icon("pencil"), [this, path] { begin_rename(path); });
	m_menu->add_item("Delete", "Del", context().icon("trash"),
					 [this, path, position] { defer([this, path, position] { confirm_delete(path, position); }); });
	m_menu->add_separator();
	m_menu->add_item(Aquila::Platform::file_manager_action_name(), {}, context().icon("folder-open"),
					 [this, path] { Aquila::Platform::show_in_file_manager(context().to_native(path)); });
	m_menu->open_at(position);
}

void ContentBrowserWindow::confirm_delete(const std::string &path, Vec2 position) {
	const std::string name = path.substr(path.find_last_of('/') + 1);
	const bool folder = VirtualFileSystem::get()->is_directory(path);
	m_confirm->clear_items();
	m_confirm->add_item(folder ? "Delete " + name + " and everything in it" : "Delete " + name, {},
						context().icon("trash"), [this, path] { remove(path); });
	m_confirm->add_item("Cancel", [] {});
	m_confirm->open_at(position);
}

void ContentBrowserWindow::remove(const std::string &path) {
	auto *vfs = VirtualFileSystem::get();
	const bool removed = vfs->is_directory(path) ? vfs->delete_directory(path, true) : vfs->delete_file_aq(path);
	if (!removed) {
		AQUILA_LOG_WARNING("ContentBrowserWindow: could not delete {}", path);
	}
	navigate(m_directory);
}

void ContentBrowserWindow::begin_rename(const std::string &path) {
	auto tile = std::ranges::find(m_tiles, path, &Tile::path);
	if (tile == m_tiles.end()) {
		return;
	}
	static_cast<ContentTile *>(tile->view)->begin_rename([this, path](Option<std::string> name) {
		if (name) {
			rename(path, *name);
		}
		navigate(m_directory);
	});
}

void ContentBrowserWindow::rename(const std::string &path, const std::string &name) {
	const std::string target = path.substr(0, path.find_last_of('/') + 1) + name;
	if (target == path) {
		return;
	}
	auto *vfs = VirtualFileSystem::get();
	if (name.empty() || name.find('/') != std::string::npos || vfs->exists(target) || vfs->is_directory(target)) {
		AQUILA_LOG_WARNING("ContentBrowserWindow: cannot rename {} to '{}'", path, name);
		return;
	}
	if (!vfs->rename_file(path, target)) {
		AQUILA_LOG_WARNING("ContentBrowserWindow: could not rename {}", path);
		return;
	}
	m_selected = target;
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

		if (double_click && tile.path == path &&
			asset_type(std::filesystem::path(tile.path).filename().string()) == "script") {
			Aquila::Platform::launch_detached("vscodium", { *VirtualFileSystem::get()->to_native_path(path) });
		}
	}
}

}
