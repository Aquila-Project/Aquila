#include <doctest.h>

#include "Aquila/UI/Core/Behaviour.h"
#include "Aquila/UI/Core/BehaviourWindow.h"
#include "Aquila/UI/Core/Canvas.h"
#include "Aquila/UI/Core/Command.h"
#include "Aquila/UI/Core/Shortcuts.h"
#include "Aquila/UI/DevTools/UIDebugPanel.h"
#include "Aquila/UI/DevTools/UIDebugWindow.h"
#include "Aquila/UI/DevTools/WidgetGalleryWindow.h"
#include "Aquila/UI/Core/TextureCache.h"
#include "Aquila/UI/Core/UIResources.h"
#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/Checkbox.h"
#include "Aquila/UI/Widgets/Clickable.h"
#include "Aquila/UI/Widgets/ColorPicker.h"
#include "Aquila/UI/Widgets/DockNode.h"
#include "Aquila/UI/Widgets/DockPanel.h"
#include "Aquila/UI/Widgets/DockSpace.h"
#include "Aquila/UI/Widgets/DragInt.h"
#include "Aquila/UI/Widgets/Dropdown.h"
#include "Aquila/UI/Widgets/Image.h"
#include "Aquila/UI/Widgets/InspectorView.h"
#include "Aquila/UI/Widgets/PropertyGrid.h"
#include "Aquila/UI/Widgets/ReflectedPropertyGrid.h"
#include "Aquila/UI/Widgets/ScrollView.h"
#include "Aquila/UI/Widgets/SelectableTextView.h"
#include "Aquila/UI/Widgets/TabBar.h"
#include "Aquila/UI/Widgets/DragFloat.h"
#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/OverlayCheckRow.h"
#include "Aquila/UI/Widgets/Popup.h"
#include "Aquila/UI/Widgets/PopupMenu.h"
#include "Aquila/UI/Widgets/TextInput.h"
#include "Aquila/UI/Widgets/TreeView.h"
#include "GpuTestSupport.h"
#include "InspectorFixture.h"
#include "LoaderTestSupport.h"
#include "StyleSnapshot.h"

using namespace Aquila::UI;
using namespace Aquila::UI::Core;

namespace {

View &snapshot_box(const LoaderCanvas &fixture, F32 width, F32 height) {
	test_gpu();
	View *box = fixture.host().add_child<View>();
	StyleProperties style;
	style.width = StyleLength::pixel(width);
	style.height = StyleLength::pixel(height);
	style.flex_direction = FlexDirection::Column;
	style.overflow = Overflow::Hidden;
	style.font_family = "regular";
	box->merge_style(style);
	return *box;
}

class ChordStandIn final : public Button {
  public:
	explicit ChordStandIn(std::string label = {}) : Button(std::move(label)) {}
	[[nodiscard]] std::string_view get_type_name() const override { return "ChordButton"; }
};

enum class SettingsShown : Uint8 { Page, Search, Shortcuts, Empty };

struct SettingsNav {
	const char *name;
	const char *icon;
};

constexpr std::array<SettingsNav, 8> K_SETTINGS_NAV = { {
	{ .name = "Interface", .icon = "palette" },
	{ .name = "Viewport", .icon = "eye" },
	{ .name = "Navigation", .icon = "video" },
	{ .name = "Gizmos", .icon = "move-3d" },
	{ .name = "Play", .icon = "play" },
	{ .name = "Content Browser", .icon = "folder" },
	{ .name = "Shortcuts", .icon = "keyboard" },
	{ .name = "Advanced", .icon = "wrench" },
} };

const Aquila::Reflection::TypeInfo &settings_search_type() {
	static const Unique<Aquila::Reflection::TypeInfo> type = [] {
		auto filtered = std::make_unique<Aquila::Reflection::TypeInfo>("Interface");
		for (const Aquila::Reflection::Property &property : InspectorFixture::everything_type().get_properties()) {
			if (property.name == "Roughness" || property.name == "Count") {
				filtered->add(property);
			}
		}
		return filtered;
	}();
	return *type;
}

Aquila::GFX::GfxTexture *settings_icon(TextureCache &icons, std::string_view name) {
	return icons.load("Engine/UI/Icons/" + std::string(name) + ".svg");
}

enum class LauncherShown : Uint8 { Recent, Delete, Empty, New };

struct LauncherProject {
	const char *name;
	const char *path;
	const char *when;
};

constexpr std::array<LauncherProject, 3> K_LAUNCHER_PROJECTS = { {
	{ .name = "Lighthouse", .path = "/home/user/Aquila/Projects/Lighthouse", .when = "2 hours ago" },
	{ .name = "Shader $1 & <b>", .path = "/mnt/data/shaders/Shader_1__b", .when = "just now" },
	{ .name = "Untitled", .path = "/home/user/Aquila/Projects/Untitled", .when = "never" },
} };

View &debug_target(View &host) {
	auto *stage = host.add_child<View>();
	stage->set_id("stage");
	stage->add_class("stage");
	stage->add_child<Label>(std::string("Title"))->add_class("stage-title");
	auto *row = stage->add_child<View>();
	row->add_class("stage-row");
	row->add_child<Button>(std::string("Go"))->set_id("go");
	row->add_child<Label>(std::string("Deep"));
	return *stage;
}

void settle_canvas(Canvas &canvas) {
	const auto settle = [&canvas] {
		for (int i = 0; i < 4; ++i) {
			canvas.update(0.016F);
			canvas.compute();
		}
	};
	settle();
	std::this_thread::sleep_for(std::chrono::milliseconds(400));
	settle();
}

TextInput *first_text_input(View &view) {
	for (const auto &child : view.get_children()) {
		if (auto *input = dynamic_cast<TextInput *>(child.get())) {
			return input;
		}
		if (TextInput *found = first_text_input(*child)) {
			return found;
		}
	}
	return nullptr;
}

enum class ShellShown : Uint8 { Idle, Playing, Menu };

struct ShellWindow {
	const char *id;
	const char *title;
	const char *icon;
};

constexpr std::array<ShellWindow, 6> K_SHELL_WINDOWS = { {
	{ .id = "viewport", .title = "Viewport", .icon = "video" },
	{ .id = "hierarchy", .title = "Hierarchy", .icon = "list-tree" },
	{ .id = "inspector", .title = "Inspector", .icon = "sliders-horizontal" },
	{ .id = "content", .title = "Content", .icon = "folder-open" },
	{ .id = "console", .title = "Console", .icon = "terminal" },
	{ .id = "game", .title = "Game", .icon = "gamepad-2" },
} };

struct ShellCommands {
	Commands commands;
	Shortcuts shortcuts{ commands };

	ShellCommands() {
		using Entry = std::pair<const char *, const char *>;
		constexpr std::array<Entry, 16> K_COMMANDS = { {
			{ "scene.new", "New scene" },
			{ "scene.open", "Open scene" },
			{ "scene.save", "Save scene" },
			{ "scene.save_as", "Save scene as" },
			{ "editor.quit", "Quit" },
			{ "editor.preferences", "Preferences" },
			{ "play.start", "Play" },
			{ "play.pause", "Pause" },
			{ "play.stop", "Stop" },
			{ "scene.reset_demo", "Reset demo scene" },
			{ "window.open.build", "Build" },
			{ "workspace.reset", "Reset workspace" },
			{ "view.status_bar", "Status bar" },
			{ "devtools.ui_inspector", "UI inspector" },
			{ "devtools.widget_gallery", "Widget gallery" },
			{ "editor.reload_styles", "Reload styles" },
		} };
		for (const auto &[id, title] : K_COMMANDS) {
			commands.add({ .id = id, .title = title, .run = [] {} });
		}
		for (const ShellWindow &window : K_SHELL_WINDOWS) {
			commands.add({ .id = std::string("window.open.") + window.id, .title = window.title, .run = [] {} });
		}
		constexpr std::array<Entry, 10> K_CHORDS = { {
			{ "scene.new", "Ctrl+N" },
			{ "scene.open", "Ctrl+O" },
			{ "scene.save", "Ctrl+S" },
			{ "scene.save_as", "Ctrl+Shift+S" },
			{ "editor.preferences", "Ctrl+," },
			{ "editor.quit", "Ctrl+Q" },
			{ "devtools.ui_inspector", "Ctrl+Shift+I" },
			{ "play.start", "Ctrl+P" },
			{ "play.pause", "Ctrl+Shift+P" },
			{ "play.stop", "Ctrl+Alt+P" },
		} };
		for (const auto &[id, chord] : K_CHORDS) {
			shortcuts.bind(id, chord);
		}
	}
};

void set_shell_play_state(View &root, bool playing) {
	if (auto *play = root.find_by_id<Button>("topbar-play")) {
		play->set_class("topbar-btn-active", playing);
	}
	if (auto *pause = root.find_by_id<Button>("topbar-pause")) {
		pause->set_enabled(playing);
		pause->set_class("topbar-btn-active", false);
	}
	if (auto *stop = root.find_by_id<Button>("topbar-stop")) {
		stop->set_enabled(playing);
	}
	for (const char *id : { "topbar", "dock-root", "hud-status", "editor-dock" }) {
		if (View *view = root.find_by_id(id)) {
			view->set_class("play-tint", playing);
		}
	}
}
}

TEST_SUITE("Style snapshots") {
	TEST_CASE("style snapshot inspector" * doctest::skip()) {
		for (const F32 width : { 260.F, 420.F }) {
			LoaderCanvas fixture(1200, 900);
			View &box = snapshot_box(fixture, width, 880.F);
			InspectorFixture::EverythingInspector inspector;
			inspector.build(fixture.loader, box, "Light");
			fixture.settle_fully();
			write_style_snapshot("inspector-" + std::to_string(static_cast<int>(width)), box);
		}
	}

	TEST_CASE("style snapshot inspector view" * doctest::skip()) {
		using InspectorFixture::Everything;
		for (const F32 width : { 260.F, 420.F }) {
			LoaderCanvas fixture(1200, 1400);
			View &box = snapshot_box(fixture, width, 1380.F);
			View *root = fixture.loader.build_file("/resources/Engine/UI/Windows/inspector.aqlayout", box);
			REQUIRE(root != nullptr);
			Everything titled;
			Everything bare;
			const auto section = [](std::string id, std::string title, Everything &instance) {
				InspectorSection result;
				result.id = std::move(id);
				result.title = std::move(title);
				result.type = &InspectorFixture::everything_type();
				result.instance = [&instance]() -> void * { return &instance; };
				return result;
			};
			Inspectable inspectable;
			inspectable.title = "Cube";
			inspectable.subtitle = "550e8400-e29b-41d4-a716-446655440000";
			inspectable.tabs.resize(3);
			inspectable.tabs[0].title = "Object";
			inspectable.tabs[0].sections.push_back(section("light", "Light", titled));
			inspectable.tabs[0].sections.back().header_actions.push_back({ .text = "Remove", .run = [] {} });
			inspectable.tabs[0].sections.push_back(section("bare", "", bare));
			inspectable.tabs[0].actions.push_back({ .text = "Add Component", .run = [] {} });
			inspectable.tabs[0].actions.push_back(
				{ .text = "Disabled", .run = [] {}, .enabled = [] { return false; } });
			inspectable.tabs[1].title = "Components";
			inspectable.tabs[1].always_visible = true;
			inspectable.tabs[2].title = "Rendering";
			inspectable.tabs[2].sections.push_back(section("shadows", "Shadows", titled));

			InspectorView view(*root, InspectorView::IconResolver{}, nullptr);
			view.show(std::move(inspectable));
			View *section_root = view.find_section("light");
			REQUIRE(section_root != nullptr);
			PropertyGrid *section_grid = nullptr;
			for (const auto &child : section_root->get_children()) {
				for (const auto &inner : child->get_children()) {
					if (auto *grid = dynamic_cast<PropertyGrid *>(inner.get())) {
						section_grid = grid;
					}
				}
			}
			REQUIRE(section_grid != nullptr);
			REQUIRE(fixture.loader.create("/resources/Engine/UI/Windows/inspector.aqlayout", "InspectorPreview",
										  *section_grid) != nullptr);
			fixture.settle_fully();
			write_style_snapshot("inspector-view-" + std::to_string(static_cast<int>(width)), box);
		}
	}

	TEST_CASE("style snapshot plain grid" * doctest::skip()) {
		for (const F32 width : { 260.F, 420.F }) {
			LoaderCanvas fixture(1200, 900);
			View &box = snapshot_box(fixture, width, 880.F);
			auto *grid = box.add_child<PropertyGrid>();
			InspectorFixture::Everything instance;
			ReflectedPropertyGrid reflected(*grid, InspectorFixture::everything_type(), nullptr);
			reflected.bind(&instance);
			fixture.settle_fully();
			write_style_snapshot("plain-grid-" + std::to_string(static_cast<int>(width)), box);
		}
	}

	TEST_CASE("style snapshot dock" * doctest::skip()) {
		LoaderCanvas fixture(1200, 900);
		View &box = snapshot_box(fixture, 640.F, 400.F);
		auto *space = box.add_child<DockSpace>();
		StyleProperties fill;
		fill.width = StyleLength::grow();
		fill.height = StyleLength::grow();
		space->merge_style(fill);
		const Aquila::Foundation::Registration registration =
			fixture.loader.behaviours().add("DockTabActions", [] { return std::make_unique<Behaviour>(); });
		space->set_tab_bar_decorator([&fixture](DockNode *, View *actions) {
			REQUIRE(fixture.loader.create(engine_component_file("dock_tab_actions"), "DockTabActions", *actions) !=
					nullptr);
		});
		DockNode *root = space->get_root_node();
		REQUIRE(root != nullptr);
		root->add_panel("first");
		root->add_panel("second");
		fixture.settle_fully();
		write_style_snapshot("dock", box);
	}

	TEST_CASE("style snapshot hierarchy" * doctest::skip()) {
		LoaderCanvas fixture(1200, 900);
		View &box = snapshot_box(fixture, 280.F, 400.F);
		const std::string file = "/resources/Engine/UI/Windows/hierarchy.aqlayout";
		const Aquila::Foundation::Registration registration =
			fixture.loader.behaviours().add("HierarchyRow", [] { return std::make_unique<Behaviour>(); });
		View *window = fixture.loader.build_file(file, box);
		REQUIRE(window != nullptr);
		View *scroll = window->find_by_id("hierarchy-scroll");
		REQUIRE(scroll != nullptr);
		auto *tree = scroll->add_child<TreeView>();
		tree->set_row_component(file, "HierarchyRow");
		const auto eye_of = [](TreeNode &node, bool hidden) {
			auto *eye = dynamic_cast<Button *>(node.get_row_component());
			REQUIRE(eye != nullptr);
			eye->set_icon(nullptr);
			eye->set_class("hierarchy-eye-shown", hidden);
			node.get_header()->set_class("hierarchy-entity-hidden", hidden);
		};
		TreeNode *parent = tree->add_node("Cube");
		eye_of(*parent, false);
		eye_of(*parent->add_child_node("Child"), false);
		eye_of(*tree->add_node("Hidden"), true);
		fixture.settle_fully();
		write_style_snapshot("hierarchy", box);
	}

	TEST_CASE("style snapshot content browser" * doctest::skip()) {
		constexpr F32 K_SIZE = 64.F;
		const std::string file = "/resources/Engine/UI/Windows/content_browser.aqlayout";
		for (const bool empty : { false, true }) {
			LoaderCanvas fixture(1200, 900);
			const Aquila::Foundation::Registration registration =
				fixture.loader.behaviours().add("ContentTile", [] { return std::make_unique<Behaviour>(); });
			View &box = snapshot_box(fixture, 520.F, 400.F);
			View *window = fixture.loader.build_file(file, box);
			REQUIRE(window != nullptr);
			View *path = window->find_by_id("content-path");
			View *grid = window->find_by_id("content-grid");
			View *empty_text = window->find_by_id("content-empty");
			REQUIRE(path != nullptr);
			REQUIRE(grid != nullptr);
			REQUIRE(empty_text != nullptr);
			const auto crumb = [&](const char *label, bool current) {
				if (!path->get_children().empty()) {
					REQUIRE(fixture.loader.create(file, "ContentCrumbSeparator", *path) != nullptr);
				}
				auto *button = dynamic_cast<Button *>(
					fixture.loader.create(file, "ContentCrumb", *path, { .attributes = { { "label", label } } }));
				REQUIRE(button != nullptr);
				button->set_class("content-crumb-current", current);
			};
			crumb("Content", false);
			crumb("Materials", false);
			crumb("Tiles $1 & <b>", true);
			const auto tile = [&](const char *name, bool folder, bool thumbnail) {
				View *root = fixture.loader.create(file, "ContentTile", *grid);
				REQUIRE(root != nullptr);
				StyleProperties tile_style;
				tile_style.width = StyleLength::pixel(K_SIZE + 40.F);
				root->merge_style(tile_style);
				View *preview = root->find_by_id("preview");
				auto *image = root->find_by_id<Image>("image");
				auto *label = root->find_by_id<Label>("name");
				REQUIRE(preview != nullptr);
				REQUIRE(image != nullptr);
				REQUIRE(label != nullptr);
				StyleProperties preview_style;
				preview_style.width = StyleLength::pixel(K_SIZE);
				preview_style.height = StyleLength::pixel(K_SIZE);
				preview->merge_style(preview_style);
				StyleProperties image_style;
				if (thumbnail) {
					image->set_class("content-tile-icon", false);
					image->set_class("content-tile-thumbnail", true);
					image_style.width = StyleLength::pixel(K_SIZE);
					image_style.height = StyleLength::pixel(K_SIZE / 2.F);
				} else {
					image->set_class("content-tile-folder", folder);
					image_style.width = StyleLength::pixel(48.F);
					image_style.height = StyleLength::pixel(48.F);
				}
				image->set_style(image_style);
				label->set_text(name);
			};
			if (!empty) {
				tile("Bricks", true, false);
				tile("Floor", false, false);
				tile("Albedo", false, true);
				tile("move_player", false, false);
			}
			empty_text->set_hidden(!empty);
			fixture.settle_fully();
			write_style_snapshot(empty ? "content-browser-empty" : "content-browser", box);
		}
	}

	TEST_CASE("style snapshot viewport popovers" * doctest::skip()) {
		LoaderCanvas fixture(1200, 900);
		for (const char *widget : { "TransformGizmo", "ViewportOverlay" }) {
			fixture.loader.register_widget(
				widget, [](std::string_view, Aquila::UI::Text::FontAtlas *) { return std::make_unique<View>(); });
		}
		View &box = snapshot_box(fixture, 800.F, 600.F);
		View *root = fixture.loader.build_file("/resources/Engine/UI/Windows/viewport.aqlayout", box);
		REQUIRE(root != nullptr);
		auto *shading = root->find_by_id<Dropdown>("viewport-shading");
		REQUIRE(shading != nullptr);
		shading->set_variant("overlay-dropdown");
		shading->add_option("0", "Lit");

		const auto check = [root](const char *id, bool checked) {
			View *row = root->find_by_id(id);
			REQUIRE(row != nullptr);
			auto *behaviour = find_behaviour<OverlayCheckRow>(*row);
			REQUIRE(behaviour != nullptr);
			REQUIRE(behaviour->get_check() != nullptr);
			behaviour->get_check()->set_checked(checked);
		};
		check("show-grid", true);
		check("show-sky", true);
		check("show-shadows", true);
		check("show-outline", true);
		check("show-realtime", false);
		check("overlay-axis-widget", true);
		for (const char *id : { "overlay-selection-label", "overlay-safe-frame", "overlay-render-status",
								"overlay-statistics", "overlay-gpu-timings" }) {
			check(id, false);
		}
		auto *show = root->find_by_id<Popup>("viewport-show-popover");
		REQUIRE(show != nullptr);
		show->open();

		auto *camera = root->find_by_id<Popup>("viewport-camera-popover");
		auto *grid = root->find_by_id<PropertyGrid>("camera-grid");
		REQUIRE(camera != nullptr);
		REQUIRE(grid != nullptr);
		grid->add_row<DragFloat>("Field of View", DragFloat::Config{ .min = 10.F, .max = 120.F, .speed = 0.25F });
		grid->add_check_row<Checkbox>("Auto Exposure", true);
		grid->add_row<Dropdown>("Tone Mapping")->set_variant("props-dropdown");
		camera->open();

		fixture.settle_fully();
		write_style_snapshot("viewport", box);
	}

	TEST_CASE("style snapshot console" * doctest::skip()) {
		LoaderCanvas fixture(1200, 900);
		View &box = snapshot_box(fixture, 640.F, 400.F);
		REQUIRE(fixture.loader.build_file("/resources/Engine/UI/Windows/console.aqlayout", box) != nullptr);
		fixture.settle_fully();
		write_style_snapshot("console", box);
	}

	TEST_CASE("style snapshot build" * doctest::skip()) {
		const std::string file = "/resources/Engine/UI/Windows/build.aqlayout";
		for (const bool found : { true, false }) {
			LoaderCanvas fixture(1200, 900);
			const Aquila::Foundation::Registration registration =
				fixture.loader.behaviours().add("BuildTemplateInfo", [] { return std::make_unique<Behaviour>(); });
			View &box = snapshot_box(fixture, 720.F, 600.F);
			View *window = fixture.loader.build_file(file, box);
			REQUIRE(window != nullptr);
			auto *grid = window->find_by_id<PropertyGrid>("build-grid");
			REQUIRE(grid != nullptr);
			const auto browse = [&](View &field, const char *tooltip) {
				REQUIRE(grid->add_action(
							field,
							fixture.loader.create_detached(file, "BuildBrowseButton",
														   { .attributes = { { "tooltip", tooltip } } })) != nullptr);
			};
			grid->add_row<TextInput>("Game Name");
			browse(*grid->add_row<TextInput>("Output Folder"), "Pick the output folder");
			grid->add_row<Dropdown>("Startup Scene");
			browse(*grid->add_row<TextInput>("Icon"), "Pick a PNG");
			grid->add_row<DragInt>("Width");
			grid->add_check_row<Checkbox>("Fullscreen", false);
			View *info = grid->add_row("Player Template", fixture.loader.create_detached(file, "BuildTemplateInfo"));
			REQUIRE(info != nullptr);
			auto *state = info->find_by_id<Label>("state");
			auto *path = info->find_by_id<SelectableTextView>("path");
			REQUIRE(state != nullptr);
			REQUIRE(path != nullptr);
			state->set_text(found ? "Found" : "Missing: build the PlayerTemplate target");
			state->set_class("build-template-found", found);
			state->set_class("build-template-missing", !found);
			path->add_line("/home/user/Aquila Studio/Templates/Player");
			fixture.settle_fully();
			write_style_snapshot(found ? "build" : "build-missing", box);
		}
	}

	TEST_CASE("style snapshot settings" * doctest::skip()) {
		using InspectorFixture::Everything;
		const std::string file = "/resources/Engine/UI/Windows/settings.aqlayout";
		for (const SettingsShown shown :
			 { SettingsShown::Page, SettingsShown::Search, SettingsShown::Shortcuts, SettingsShown::Empty }) {
			LoaderCanvas fixture(1200, 900);
			TextureCache icons(*test_gpu().ctx, "/resources");
			fixture.loader.register_texture_cache(&icons);
			fixture.loader.register_widget("ChordButton", [](std::string_view text, Text::FontAtlas *) {
				return std::make_unique<ChordStandIn>(std::string(text));
			});
			std::vector<Aquila::Foundation::Registration> registrations;
			for (const char *behaviour : { "SettingsSection", "SettingsShortcutRow" }) {
				registrations.push_back(
					fixture.loader.behaviours().add(behaviour, [] { return std::make_unique<Behaviour>(); }));
			}
			View &box = snapshot_box(fixture, 860.F, 640.F);
			View *window = fixture.loader.build_file(file, box);
			REQUIRE(window != nullptr);
			auto *note = window->find_by_id<Label>("settings-footer-note");
			auto *scopes = window->find_by_id<TabBar>("settings-scopes");
			View *nav = window->find_by_id("settings-nav");
			View *content = window->find_by_id("settings-content");
			REQUIRE(note != nullptr);
			REQUIRE(scopes != nullptr);
			REQUIRE(nav != nullptr);
			REQUIRE(content != nullptr);
			note->set_text("Saved for you in editor_preferences.json, for every project.");
			scopes->set_active(0);

			const bool searching = shown == SettingsShown::Search || shown == SettingsShown::Empty;
			const Usize selected = shown == SettingsShown::Shortcuts ? 6 : 0;
			for (Usize i = 0; i < K_SETTINGS_NAV.size(); ++i) {
				View *item = fixture.loader.create(
					file, "SettingsNavItem", *nav,
					{ .attributes = { { "label", K_SETTINGS_NAV[i].name }, { "icon", K_SETTINGS_NAV[i].icon } } });
				REQUIRE(item != nullptr);
				item->set_class("settings-nav-item-active", !searching && i == selected);
			}

			const auto section = [&](const std::string &title, const std::string &description, bool full,
									 const std::string &reset_text) -> View & {
				View *created = fixture.loader.create(file, "SettingsSection", *content);
				REQUIRE(created != nullptr);
				auto *title_label = created->find_by_id<Label>("title");
				auto *description_label = created->find_by_id<Label>("description");
				auto *reset = created->find_by_id<Button>("reset");
				REQUIRE(title_label != nullptr);
				REQUIRE(description_label != nullptr);
				REQUIRE(reset != nullptr);
				title_label->set_text(title);
				title_label->set_class("settings-page-title", full);
				title_label->set_class("settings-section-title", !full);
				description_label->set_text(description);
				description_label->set_hidden(!full || description.empty());
				reset->set_text(reset_text);
				reset->set_hidden(reset_text.empty());
				return *created;
			};
			const auto add = [&](View &host, View &before, const char *component, ComponentArgs args = {}) -> View & {
				View *created = fixture.loader.create(file, component, host, std::move(args));
				REQUIRE(created != nullptr);
				host.reorder_child(created, &before);
				return *created;
			};

			Everything instance;
			Unique<ReflectedPropertyGrid> reflected;
			if (shown == SettingsShown::Page || shown == SettingsShown::Search) {
				const bool full = shown == SettingsShown::Page;
				View &page =
					section("Interface", "Fonts and size of the whole editor.", full, full ? "Reset to Defaults" : "");
				auto *grid = page.find_by_id<PropertyGrid>("grid");
				REQUIRE(grid != nullptr);
				reflected = std::make_unique<ReflectedPropertyGrid>(
					*grid, full ? InspectorFixture::everything_type() : settings_search_type(), nullptr);
				reflected->bind(&instance);
			} else if (shown == SettingsShown::Shortcuts) {
				View &page =
					section("Shortcuts", "Click a shortcut and press the new keys.", true, "Reset All Shortcuts");
				page.find_by_id("grid")->set_hidden(true);
				View &page_reset = *page.find_by_id("reset");
				add(page, page_reset, "SettingsShortcutGroup", { .attributes = { { "title", "Scene" } } });
				for (const bool bound : { true, false }) {
					View &row = add(page, page_reset, "SettingsShortcutRow");
					auto *title = row.find_by_id<Label>("title");
					auto *chord = row.find_by_id<Button>("chord");
					auto *reset = row.find_by_id<Button>("reset");
					REQUIRE(title != nullptr);
					REQUIRE(chord != nullptr);
					REQUIRE(reset != nullptr);
					title->set_text(bound ? "Save Scene" : "Open Scene");
					chord->set_text(bound ? "Ctrl+S" : "Not set");
					chord->set_class("settings-chord-empty", !bound);
					reset->set_hidden(!bound);
					if (bound) {
						add(page, page_reset, "SettingsShortcutClash",
							{ .attributes = { { "text", "Also used by Save Scene As" } } });
					}
				}
			} else {
				REQUIRE(fixture.loader.create(file, "SettingsEmpty", *content,
											  { .attributes = { { "text", "Nothing matches \"zzz\"." } } }) != nullptr);
			}

			fixture.settle_fully();
			constexpr std::array<const char *, 4> K_NAMES = { "settings-page", "settings-search", "settings-shortcuts",
															  "settings-empty" };
			write_style_snapshot(K_NAMES.at(static_cast<Usize>(shown)), box);
		}
	}

	TEST_CASE("style snapshot launcher" * doctest::skip()) {
		const std::string file = "/resources/Engine/UI/Windows/launcher.aqlayout";
		for (const LauncherShown shown :
			 { LauncherShown::Recent, LauncherShown::Delete, LauncherShown::Empty, LauncherShown::New }) {
			LoaderCanvas fixture(1200, 900);
			TextureCache icons(*test_gpu().ctx, "/resources");
			fixture.loader.register_texture_cache(&icons);
			View &box = snapshot_box(fixture, 900.F, 600.F);
			View *window = fixture.loader.build_file(file, box);
			REQUIRE(window != nullptr);
			const auto find = [window](const char *id) {
				View *found = window->find_by_id(id);
				REQUIRE_MESSAGE(found != nullptr, id);
				return found;
			};
			dynamic_cast<Label &>(*find("launcher-version")).set_text("Engine 0.1.0");
			dynamic_cast<TextInput &>(*find("launcher-name")).set_text("Lighthouse 2");
			dynamic_cast<TextInput &>(*find("launcher-location")).set_text("/home/user/Aquila/Projects");
			dynamic_cast<Label &>(*find("launcher-preview")).set_text("/home/user/Aquila/Projects/Lighthouse_2");

			const bool has_projects = shown != LauncherShown::Empty;
			find("launcher-empty")->set_class("hidden", has_projects);
			find("launcher-recent-scroll")->set_class("hidden", !has_projects);
			if (has_projects) {
				for (const LauncherProject &project : K_LAUNCHER_PROJECTS) {
					REQUIRE(fixture.loader.create(file, "LauncherRecent", *find("launcher-recent"),
												  { .attributes = { { "name", project.name },
																	{ "path", project.path },
																	{ "when", project.when } } }) != nullptr);
				}
			}
			if (shown == LauncherShown::Delete) {
				find("launcher-action-bar")->set_class("hidden", false);
				find("launcher-action-input")->set_class("hidden", true);
				auto &confirm = dynamic_cast<Button &>(*find("launcher-action-confirm"));
				confirm.set_class("launcher-danger", true);
				confirm.set_text("Delete");
				dynamic_cast<Label &>(*find("launcher-action-label"))
					.set_text("Delete 'Lighthouse' and all of its files? This cannot be undone.");
			}
			const bool new_page = shown == LauncherShown::New;
			find("launcher-page-home")->set_class("hidden", new_page);
			find("launcher-page-new")->set_class("hidden", !new_page);

			fixture.settle_fully();
			constexpr std::array<const char *, 4> K_NAMES = { "launcher-recent", "launcher-delete", "launcher-empty",
															  "launcher-new" };
			write_style_snapshot(K_NAMES.at(static_cast<Usize>(shown)), box);
		}
	}

	TEST_CASE("style snapshot widget gallery" * doctest::skip()) {
		LoaderCanvas fixture(1200, 900);
		TextureCache icons(*test_gpu().ctx, "/resources");
		fixture.loader.register_texture_cache(&icons);
		fixture.loader.register_widget("ColorPicker", [](std::string_view, Text::FontAtlas *) -> Unique<View> {
			return std::make_unique<ColorPicker>(*test_gpu().ctx);
		});
		BehaviourWindow gallery(fixture.loader, std::make_unique<DevTools::WidgetGalleryWindow>(), 420, 720);
		Canvas &canvas = *gallery.get_canvas();
		const auto settle = [&canvas] {
			for (int i = 0; i < 4; ++i) {
				canvas.update(0.016F);
				canvas.compute();
			}
		};
		settle();
		std::this_thread::sleep_for(std::chrono::milliseconds(400));
		settle();
		View *root = canvas.get_root()->find_by_id("gallery-root");
		REQUIRE(root != nullptr);
		write_style_snapshot("widget-gallery", *root);
	}

	TEST_CASE("style snapshot ui debug window" * doctest::skip()) {
		for (const bool filtered : { false, true }) {
			LoaderCanvas fixture(1200, 900);
			test_gpu();
			View &stage = debug_target(fixture.host());
			fixture.settle();
			BehaviourWindow window(fixture.loader, std::make_unique<DevTools::UIDebugWindow>(*fixture.canvas), 800,
								   600);
			auto *inspector = window.get_behaviour<DevTools::UIDebugWindow>();
			REQUIRE(inspector != nullptr);
			inspector->select_view(stage.find_by_id("go"));
			if (filtered) {
				auto *search = window.get_canvas()->get_root()->find_by_id<TextInput>("inspector-filter");
				REQUIRE(search != nullptr);
				search->on_changed("color");
			}
			settle_canvas(*window.get_canvas());
			View *root = window.get_canvas()->get_root()->find_by_id("inspector-root");
			REQUIRE(root != nullptr);
			write_style_snapshot(filtered ? "ui-debug-filtered" : "ui-debug", *root);
		}
	}

	TEST_CASE("style snapshot ui debug panel" * doctest::skip()) {
		LoaderCanvas fixture(1200, 900);
		View &box = snapshot_box(fixture, 800.F, 600.F);
		debug_target(box);
		auto *panel = fixture.loader.create<DevTools::UIDebugPanel>("Components/ui_debug_panel", "UIDebugPanel", box);
		REQUIRE(panel != nullptr);
		panel->set_target(fixture.canvas.get());
		panel->toggle();
		fixture.settle_fully();
		View *window = box.find_by_id("ui-debug-window");
		REQUIRE(window != nullptr);
		write_style_snapshot("ui-debug-panel", *window);
	}

	TEST_CASE("style snapshot editor shell" * doctest::skip()) {
		for (const ShellShown shown : { ShellShown::Idle, ShellShown::Playing, ShellShown::Menu }) {
			LoaderCanvas fixture(1400, 900);
			TextureCache icons(*test_gpu().ctx, "/resources");
			fixture.loader.register_texture_cache(&icons);
			ShellCommands shell;
			fixture.loader.set_command_source(&shell.commands, &shell.shortcuts);
			View &box = snapshot_box(fixture, 1280.F, 720.F);
			View *root = fixture.loader.build_file("/resources/Engine/UI/Windows/editor.aqlayout", box);
			REQUIRE(root != nullptr);
			auto *tabs = root->find_by_id<TabBar>("workspace-tabs");
			REQUIRE(tabs != nullptr);
			for (const char *workspace : { "Scene", "Layout", "Debug", "Game" }) {
				tabs->add_tab(workspace);
			}
			tabs->set_active(0);

			const auto icon = [&icons](std::string_view name) {
				return icons.load("Engine/UI/Icons/" + std::string(name) + ".svg");
			};
			auto *menu = root->find_by_id<PopupMenu>("main-menu");
			auto *window_menu = root->find_by_id<PopupMenu>("window-menu");
			REQUIRE(menu != nullptr);
			REQUIRE(window_menu != nullptr);
			menu->set_submenu_icon(icon("chevron-right"));
			Usize index = 0;
			for (const ShellWindow &window : K_SHELL_WINDOWS) {
				const std::string command = std::string("window.open.") + window.id;
				window_menu->insert_item(index++, shell.commands.find(command)->title,
										 shell.shortcuts.get_label(command), icon(window.icon), [] {});
			}

			set_shell_play_state(*root, shown == ShellShown::Playing);
			fixture.settle();
			if (shown == ShellShown::Menu) {
				auto *menu_button = root->find_by_id<Button>("topbar-menu");
				REQUIRE(menu_button != nullptr);
				const Rect rect = menu_button->get_absolute_rect();
				menu->open_at({ rect.position.x, rect.position.y + rect.size.y + 4.F });
			}
			fixture.settle_fully();
			constexpr std::array<const char *, 3> K_NAMES = { "shell", "shell-playing", "shell-menu" };
			write_style_snapshot(K_NAMES.at(static_cast<Usize>(shown)), *root);
		}
	}
}
