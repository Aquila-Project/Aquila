#include "UI/EditorWindows/ContentBrowserWindow.h"

#include "Aquila/Foundation/FrameScheduler.h"
#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/Foundation/Text/StringUtils.h"
#include "Aquila/GFX/GfxTexture.h"
#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"
#include "Aquila/Platform/Process.h"
#include "Aquila/UI/Core/TextureCache.h"
#include "Aquila/UI/Widgets/AssetPayload.h"
#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/Clickable.h"
#include "Aquila/UI/Widgets/PopupMenu.h"
#include "Aquila/UI/Widgets/TextInput.h"
#include "Core/AssetThumbnails.h"
#include "Core/EditorConfig.h"
#include "Core/FileDialog.h"
#include "UI/Components/ContentTile.h"

namespace Editor {

using namespace Aquila::UI::Core;
using Aquila::Platform::KeyCode;
using Aquila::Platform::Filesystem::VirtualFileSystem;

namespace {

constexpr std::string_view K_ROOT = Aquila::SharedConstants::PROJECT_CONTENT_DIR;
constexpr std::chrono::milliseconds K_DOUBLE_CLICK{ 400 };

} // namespace

ContentBrowserWindow::ContentBrowserWindow(EditorContext &context)
	: EditorWindow(context, context.window_layout("content_browser")), m_directory(K_ROOT) {}

void ContentBrowserWindow::define_behaviour() {
	m_path = get("content-path");
	m_grid = get("content-grid");
	m_filter = get<TextInput>("content-filter");
	m_empty = get("content-empty");
	if (m_path == nullptr || m_grid == nullptr || m_filter == nullptr) {
		return;
	}

	m_filter->set_placeholder("Filter");
	m_filter->on_changed.connect([this](const std::string &) { apply_filter(); });
	if (auto *up = get<Button>("content-up")) {
		up->on_click.connect([this] {
			if (m_directory != K_ROOT) {
				navigate(m_directory.substr(0, m_directory.find_last_of('/')));
			}
		});
	}
	if (auto *refresh = get<Button>("content-refresh")) {
		refresh->on_click.connect([this] { navigate(m_directory); });
	}

	m_menu = overlay_root()->add_child(std::make_unique<PopupMenu>());
	m_confirm = overlay_root()->add_child(std::make_unique<PopupMenu>());
	m_grid->on_context_menu.connect([this](Vec2 position) { open_background_menu(position); });
	listen(context().thumbnails().on_ready, [this](const std::string &path) {
		for (const Tile &tile : m_tiles) {
			if (tile.path == path && !tile.is_directory) {
				tile.tile->set_thumbnail(context().thumbnails().get(path));
			}
		}
	});
	if (auto *scroll = get("content-scroll")) {
		scroll->on_context_menu.connect([this](Vec2 position) { open_background_menu(position); });
	}
	m_prefs = Config::get_preferences().content_browser;
	listen(context().on_settings_changed, [this] {
		if (Config::get_preferences().content_browser != m_prefs) {
			m_prefs = Config::get_preferences().content_browser;
			navigate(m_directory);
		}
	});

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
	const bool inside = directory == K_ROOT || directory.starts_with(std::string(K_ROOT) + "/");
	m_directory = inside && VirtualFileSystem::get()->is_directory(directory) ? directory : std::string(K_ROOT);
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
	m_path->clear_children();

	const auto add_crumb = [this](std::string label, std::string path) {
		if (!m_path->get_children().empty()) {
			Behaviour::create("ContentCrumbSeparator", *m_path);
		}
		auto *crumb = dynamic_cast<Button *>(
			Behaviour::create("ContentCrumb", *m_path, { .attributes = { { "label", std::move(label) } } }));
		if (crumb == nullptr) {
			return;
		}
		crumb->set_class("content-crumb-current", path == m_directory);
		crumb->on_click.connect([this, path = std::move(path)] { navigate(path); });
	};

	add_crumb(std::string(Aquila::SharedConstants::PROJECT_CONTENT_FOLDER), std::string(K_ROOT));
	Usize start = K_ROOT.size() + 1;
	while (start <= m_directory.size()) {
		const Usize end = std::min(m_directory.find('/', start), m_directory.size());
		add_crumb(m_directory.substr(start, end - start), m_directory.substr(0, end));
		start = end + 1;
	}
}

void ContentBrowserWindow::build_grid() {
	for (const Tile &tile : m_tiles) {
		m_grid->remove_child(tile.view);
	}
	m_tiles.clear();

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
		return Aquila::Foundation::to_lower(a.name) < Aquila::Foundation::to_lower(b.name);
	});

	for (Tile &tile : tiles) {
		const AssetType *asset = tile.is_directory ? nullptr : context().asset_types.find_for_path(tile.name);
		const std::string type = tile.is_directory ? "folder" : (asset != nullptr ? asset->id : "file");
		const std::string icon = tile.is_directory ? "folder" : (asset != nullptr ? asset->icon : "file");
		Aquila::GFX::GfxTexture *thumbnail = tile.is_directory ? nullptr : context().thumbnails().get(tile.path);
		tile.tile = Behaviour::create<ContentTile>("ContentTile", *m_grid);
		if (tile.tile == nullptr) {
			continue;
		}
		tile.tile->show(AssetPayload{ tile.path, type, tile.name }, tile.is_directory,
						thumbnail != nullptr ? thumbnail : context().icon(icon), thumbnail != nullptr, m_prefs);
		tile.view = &tile.tile->root();
		tile.view->set_class("content-tile-selected", tile.path == m_selected);
		tile.view->on_pressed.connect([this, path = tile.path](Vec2) { press(path); });
		tile.view->on_context_menu.connect([this, path = tile.path](Vec2 position) { open_item_menu(path, position); });
		if (Clickable *clickable = tile.tile->get_clickable()) {
			clickable->on_click.connect([this, path = tile.path] { open_in_inspector(path); });
			clickable->on_key_pressed.connect([this, path = tile.path, view = tile.view](KeyCode key, int) {
				if (key == KeyCode::F2) {
					begin_rename(path);
				} else if (key == KeyCode::Delete) {
					const Rect rect = view->get_absolute_rect();
					confirm_delete(path, { rect.position.x, rect.position.y + rect.size.y });
				}
			});
		}
		m_tiles.push_back(std::move(tile));
	}

	if (m_empty != nullptr) {
		m_empty->set_hidden(!m_tiles.empty());
	}
	apply_filter();
}

void ContentBrowserWindow::apply_filter() {
	const std::string query = Aquila::Foundation::to_lower(m_filter->get_text());
	for (const Tile &tile : m_tiles) {
		tile.view->set_hidden(!query.empty() &&
							  Aquila::Foundation::to_lower(tile.name).find(query) == std::string::npos);
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
		const std::string target =
			VirtualFileSystem::get()->unique_path(m_directory, std::filesystem::path(file).filename().string());
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
	const std::string path = VirtualFileSystem::get()->unique_path(m_directory, creator.name);
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
	if (const AssetType *type = context().asset_types.find_for_path(path); type != nullptr && type->open) {
		m_menu->add_item("Open", {}, context().icon(type->icon), [open = type->open, path] { open(path); });
		m_menu->add_separator();
	}
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
	tile->tile->start_rename([this, path](Option<std::string> name) {
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
	const bool double_click = path == m_selected && now - m_last_press < K_DOUBLE_CLICK;
	m_last_press = now;
	m_selected = path;

	for (const Tile &tile : m_tiles) {
		tile.view->set_class("content-tile-selected", tile.path == path);
		if (double_click && tile.path == path && tile.is_directory) {
			navigate(path);
		}

		if (double_click && tile.path == path && !tile.is_directory) {
			if (const AssetType *asset = context().asset_types.find_for_path(tile.path);
				asset != nullptr && asset->open) {
				asset->open(tile.path);
			}
		}
	}
}

void ContentBrowserWindow::open_in_inspector(const std::string &path) {
	if (const AssetType *asset = context().asset_types.find_for_path(path); asset != nullptr && asset->inspect) {
		context().selection.select_asset(path);
	}
}

} // namespace Editor
