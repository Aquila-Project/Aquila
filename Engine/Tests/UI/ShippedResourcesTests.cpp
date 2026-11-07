#include <doctest.h>

#include "Aquila/UI/Core/Behaviour.h"
#include "Aquila/UI/Core/Command.h"
#include "Aquila/UI/Core/LayoutLoader.h"
#include "Aquila/UI/Core/UIResources.h"
#include "Aquila/UI/Core/View.h"
#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/Image.h"
#include "Aquila/UI/Widgets/Clickable.h"
#include "Aquila/UI/Widgets/Checkbox.h"
#include "Aquila/UI/Widgets/IconLabel.h"
#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/OverlayCheckRow.h"
#include "Aquila/UI/Widgets/Popup.h"
#include "Aquila/UI/Widgets/PopupMenu.h"
#include "Aquila/UI/Widgets/PropertyGrid.h"
#include "Aquila/UI/Widgets/TabBar.h"
#include "Aquila/UI/Widgets/TextInput.h"
#include "Aquila/UI/Widgets/TreeView.h"
#include "Aquila/UI/Widgets/SelectableTextView.h"
#include "Aquila/UI/Style/StyleParser.h"
#include "Aquila/UI/Style/StyleSheet.h"
#include "VfsTestSupport.h"

using namespace Aquila::UI;

namespace {

const std::filesystem::path K_UI_DIR = std::filesystem::path(AQUILA_SOURCE_DIR) / "Resources/Engine/UI";

std::vector<std::filesystem::path> files_with(std::string_view extension) {
	std::vector<std::filesystem::path> files;
	for (const auto &entry : std::filesystem::recursive_directory_iterator(K_UI_DIR)) {
		if (entry.path().extension() == extension) {
			files.push_back(entry.path());
		}
	}
	std::ranges::sort(files, [](const auto &a, const auto &b) {
		return (a.stem() == "tokens") != (b.stem() == "tokens") ? a.stem() == "tokens" : a < b;
	});
	return files;
}

std::string vfs_path(const std::filesystem::path &file) {
	return "/resources/Engine/UI/" + std::filesystem::relative(file, K_UI_DIR).generic_string();
}

void register_editor_widget_stand_ins(Core::LayoutLoader &loader) {
	for (const char *widget : { "ChordButton", "ColorPicker", "RenameField", "TransformGizmo", "ViewportOverlay" }) {
		loader.register_widget(widget,
							   [](std::string_view, Text::FontAtlas *) { return std::make_unique<Core::View>(); });
	}
}
}

TEST_SUITE("Shipped UI resources") {
	TEST_CASE("every stylesheet parses without diagnostics") {
		test_vfs();
		const auto files = files_with(".aqstyle");
		REQUIRE_FALSE(files.empty());

		std::vector<std::string> paths;
		for (const auto &file : files) {
			paths.push_back(vfs_path(file));
		}
		StyleSheet sheet;
		LogCapture capture;
		CHECK(StyleParser::load_files(paths, sheet));
		CHECK_MESSAGE(capture.stream.str().empty(), capture.stream.str());
		CHECK(sheet.get_rule_count() > 0);
	}

	TEST_CASE("every layout parses without markup diagnostics") {
		test_vfs();
		const auto files = files_with(".aqlayout");
		REQUIRE_FALSE(files.empty());

		for (const auto &file : files) {
			const std::string path = vfs_path(file);
			LogCapture capture;
			Core::LayoutLoader loader;
			register_editor_widget_stand_ins(loader);
			const auto root = loader.load_file(path);
			if (file.parent_path().filename() != "Components") {
				CHECK_MESSAGE(root != nullptr, path);
			}
			const std::string log = capture.stream.str();
			CHECK_MESSAGE(log.find(path + ":") == std::string::npos, (path + "\n" + log));
		}
	}

	TEST_CASE("the old shared sheets are gone, windows and components carry their own") {
		for (const char *sheet : { "editor_chrome.aqstyle", "launcher.aqstyle", "devtools.aqstyle" }) {
			CHECK_FALSE_MESSAGE(std::filesystem::exists(K_UI_DIR / sheet), sheet);
		}
		for (const char *sheet : { "tokens.aqstyle", "widgets.aqstyle", "dock.aqstyle" }) {
			CHECK_MESSAGE(std::filesystem::exists(K_UI_DIR / sheet), sheet);
		}
	}

	TEST_CASE("every window has its layout and its style in Windows") {
		CHECK(std::filesystem::exists(K_UI_DIR / "Windows/game.aqlayout"));
		for (const auto &file : files_with(".aqlayout")) {
			if (file.parent_path().filename() == "Windows") {
				CHECK_MESSAGE(std::filesystem::exists(std::filesystem::path(file).replace_extension(".aqstyle")),
							  file.string());
			}
		}
	}

	TEST_CASE("log classes keep their colors") {
		test_vfs();
		Aquila::UI::StyleSheet sheet;
		const std::vector<std::string> paths{
			"/resources/Engine/UI/tokens.aqstyle",
			"/resources/Engine/UI/widgets.aqstyle",
			"/resources/Engine/UI/dock.aqstyle",
			"/resources/Engine/UI/Windows/console.aqstyle",
			"/resources/Engine/UI/Windows/build.aqstyle",
		};
		REQUIRE(Aquila::UI::StyleParser::load_files(paths, sheet));
		const std::vector<std::pair<std::string, Vec4>> expected{
			{ "console-tag-info", { 0.35F, 0.87F, 0.35F, 1.F } },
			{ "console-tag-warning", { 0.83F, 0.67F, 0.29F, 1.F } },
			{ "console-tag-error", { 0.87F, 0.45F, 0.45F, 1.F } },
			{ "console-tag-critical", { 1.F, 0.32F, 0.32F, 1.F } },
			{ "build-log-info", { 0.62F, 0.62F, 0.62F, 1.F } },
			{ "build-log-warning", { 0.83F, 0.67F, 0.29F, 1.F } },
			{ "build-log-error", { 0.87F, 0.45F, 0.45F, 1.F } },
			{ "build-log-success", { 0.29F, 0.72F, 0.44F, 1.F } },
		};
		for (const auto &[name, color] : expected) {
			CAPTURE(name);
			CHECK(sheet.find_class_color(name) == color);
		}
	}

	TEST_CASE("the console layout holds its filters and its log") {
		test_vfs();
		LogCapture capture;
		Core::LayoutLoader loader;
		const auto root = loader.load_file("/resources/Engine/UI/Windows/console.aqlayout");
		REQUIRE(root != nullptr);
		for (const char *id : { "console-filter-info", "console-filter-warning", "console-filter-error",
								"console-count-info", "console-count-warning", "console-count-error" }) {
			CHECK_MESSAGE(root->find_by_id(id) != nullptr, id);
		}
		CHECK(dynamic_cast<Core::SelectableTextView *>(root->find_by_id("console-log")) != nullptr);
	}

	TEST_CASE("the build layout holds its settings, log and buttons") {
		test_vfs();
		LogCapture capture;
		Core::LayoutLoader loader;
		const auto root = loader.load_file("/resources/Engine/UI/Windows/build.aqlayout");
		REQUIRE(root != nullptr);
		for (const char *id : { "build-settings", "build-status", "build-log", "build-close", "build-preview",
								"build-and-run", "build-start" }) {
			CHECK_MESSAGE(root->find_by_id(id) != nullptr, id);
		}
		CHECK(dynamic_cast<Core::SelectableTextView *>(root->find_by_id("build-log-text")) != nullptr);
	}

	TEST_CASE("the dock tab actions build their change and add buttons") {
		struct Probe final : Core::Behaviour {
			Core::Button *change = nullptr;
			Core::Button *add = nullptr;
			void define_behaviour() override {
				change = get<Core::Button>("change");
				add = get<Core::Button>("add");
			}
		};
		test_vfs();
		LogCapture capture;
		Core::LayoutLoader loader;
		const Aquila::Foundation::Registration registration = loader.behaviours().add<Probe>("DockTabActions");
		Core::View parent;
		auto *probe = loader.create<Probe>(Core::engine_component_file("dock_tab_actions"), "DockTabActions", parent);
		REQUIRE(probe != nullptr);
		CHECK(probe->change != nullptr);
		CHECK(probe->add != nullptr);
		CHECK(std::filesystem::exists(K_UI_DIR / "Components/dock_tab_actions.aqstyle"));
	}

	TEST_CASE("the inspector layout holds what InspectorView needs and its preview") {
		test_vfs();
		LogCapture capture;
		Core::LayoutLoader loader;
		const std::string file = "/resources/Engine/UI/Windows/inspector.aqlayout";
		const auto root = loader.load_file(file);
		REQUIRE(root != nullptr);
		CHECK(root->get_id() == "inspector-root");
		for (const char *id : { "props-tabs", "props-head", "props-head-icon", "inspector-name", "inspector-uuid",
								"inspector-empty", "inspector-scroll", "inspector-pages" }) {
			CHECK_MESSAGE(root->find_by_id(id) != nullptr, id);
		}
		Core::View parent;
		CHECK(dynamic_cast<Core::Image *>(loader.create(file, "InspectorPreview", parent)) != nullptr);
	}

	TEST_CASE("the hierarchy layout holds its toolbar, its scroll and its row components") {
		test_vfs();
		LogCapture capture;
		Core::LayoutLoader loader;
		const Aquila::Foundation::Registration registration =
			loader.behaviours().add("HierarchyRow", [] { return std::make_unique<Core::Behaviour>(); });
		const std::string file = "/resources/Engine/UI/Windows/hierarchy.aqlayout";
		const auto root = loader.load_file(file);
		REQUIRE(root != nullptr);
		CHECK(root->get_id() == "hierarchy-root");
		for (const char *id : { "hierarchy-toolbar", "hierarchy-filter", "hierarchy-create", "hierarchy-scroll" }) {
			CHECK_MESSAGE(root->find_by_id(id) != nullptr, id);
		}
		Core::View parent;
		CHECK(dynamic_cast<Core::Button *>(loader.create(file, "HierarchyRow", parent)) != nullptr);
	}

	TEST_CASE("the content browser layout holds its toolbar, grid, empty text and components") {
		test_vfs();
		LogCapture capture;
		Core::LayoutLoader loader;
		const Aquila::Foundation::Registration registration =
			loader.behaviours().add("ContentTile", [] { return std::make_unique<Core::Behaviour>(); });
		loader.register_widget("RenameField",
							   [](std::string_view, Text::FontAtlas *) { return std::make_unique<Core::TextInput>(); });
		const std::string file = "/resources/Engine/UI/Windows/content_browser.aqlayout";
		const auto root = loader.load_file(file);
		REQUIRE(root != nullptr);
		CHECK(root->get_id() == "content-root");
		for (const char *id : { "content-toolbar", "content-up", "content-path", "content-filter", "content-refresh",
								"content-scroll", "content-grid", "content-empty" }) {
			CHECK_MESSAGE(root->find_by_id(id) != nullptr, id);
		}

		Core::View parent;
		Core::View *tile = loader.create(file, "ContentTile", parent);
		REQUIRE(dynamic_cast<Core::Clickable *>(tile) != nullptr);
		CHECK(tile->find_by_id("preview") != nullptr);
		CHECK(tile->find_by_id<Core::Image>("image") != nullptr);
		CHECK(tile->find_by_id<Core::Label>("name") != nullptr);
		CHECK(dynamic_cast<Core::TextInput *>(loader.create(file, "ContentRename", parent)) != nullptr);
		CHECK(dynamic_cast<Core::Image *>(loader.create(file, "ContentCrumbSeparator", parent)) != nullptr);

		Core::View *crumb =
			loader.create(file, "ContentCrumb", parent, { .attributes = { { "label", "Tiles $1 & <b>" } } });
		REQUIRE(dynamic_cast<Core::Button *>(crumb) != nullptr);
		bool written = false;
		const auto find = [&written](const auto &self, const Core::View &view) -> void {
			for (const auto &child : view.get_children()) {
				if (const auto *label = dynamic_cast<const Core::Label *>(child.get())) {
					written = written || label->get_text() == "Tiles $1 & <b>";
				}
				self(self, *child);
			}
		};
		find(find, *crumb);
		CHECK(written);
	}

	TEST_CASE("the viewport layout holds its toolbars, popovers, rows and canvases") {
		test_vfs();
		LogCapture capture;
		Core::LayoutLoader loader;
		register_editor_widget_stand_ins(loader);
		CHECK(std::filesystem::exists(K_UI_DIR / "Components/overlay.aqstyle"));
		const auto root = loader.load_file("/resources/Engine/UI/Windows/viewport.aqlayout");
		REQUIRE(root != nullptr);
		CHECK(root->get_id() == "viewport-root");
		for (const char *id : { "viewport-image", "viewport-transform-tools", "tool-translate", "tool-rotate",
								"tool-scale", "tool-space", "tool-snap", "viewport-view-tools", "viewport-shading",
								"viewport-show", "viewport-show-button", "viewport-camera", "viewport-camera-button",
								"viewport-gizmo", "viewport-overlay" }) {
			CHECK_MESSAGE(root->find_by_id(id) != nullptr, id);
		}
		CHECK(root->find_by_id<Core::Popup>("viewport-show-popover") != nullptr);
		CHECK(root->find_by_id<Core::Popup>("viewport-camera-popover") != nullptr);
		const auto *grid = root->find_by_id<Core::PropertyGrid>("camera-grid");
		REQUIRE(grid != nullptr);
		CHECK(grid->is_split());
		for (const char *id : { "show-grid", "show-sky", "show-shadows", "show-outline", "show-realtime",
								"overlay-axis-widget", "overlay-selection-label", "overlay-safe-frame",
								"overlay-render-status", "overlay-statistics", "overlay-gpu-timings" }) {
			Core::View *row = root->find_by_id(id);
			REQUIRE_MESSAGE(row != nullptr, id);
			CHECK_MESSAGE(Core::find_behaviour<Core::OverlayCheckRow>(*row) != nullptr, id);
		}
	}

	TEST_CASE("the build layout holds its form grid, browse button and template info") {
		test_vfs();
		LogCapture capture;
		Core::LayoutLoader loader;
		const Aquila::Foundation::Registration registration =
			loader.behaviours().add("BuildTemplateInfo", [] { return std::make_unique<Core::Behaviour>(); });
		const std::string file = "/resources/Engine/UI/Windows/build.aqlayout";
		const auto root = loader.load_file(file);
		REQUIRE(root != nullptr);
		const auto *grid = root->find_by_id<Core::PropertyGrid>("build-grid");
		REQUIRE(grid != nullptr);
		CHECK(grid->get_label_width() == 120.F);
		CHECK(grid->is_split());

		const Unique<Core::View> browse =
			loader.create_detached(file, "BuildBrowseButton", { .attributes = { { "tooltip", "Pick $1 & <b>" } } });
		REQUIRE(dynamic_cast<Core::Button *>(browse.get()) != nullptr);
		CHECK(browse->get_tooltip() == "Pick $1 & <b>");
		const Unique<Core::View> info = loader.create_detached(file, "BuildTemplateInfo");
		REQUIRE(info != nullptr);
		CHECK(info->find_by_id<Core::Label>("state") != nullptr);
		CHECK(info->find_by_id<Core::SelectableTextView>("path") != nullptr);
	}

	TEST_CASE("the settings layout holds its header, pages, footer and components") {
		test_vfs();
		LogCapture capture;
		Core::LayoutLoader loader;
		register_editor_widget_stand_ins(loader);
		std::vector<Aquila::Foundation::Registration> registrations;
		for (const char *behaviour : { "SettingsSection", "SettingsShortcutRow" }) {
			registrations.push_back(
				loader.behaviours().add(behaviour, [] { return std::make_unique<Core::Behaviour>(); }));
		}
		const std::string file = "/resources/Engine/UI/Windows/settings.aqlayout";
		const auto root = loader.load_file(file);
		REQUIRE(root != nullptr);
		CHECK(root->get_id() == "settings-root");
		const auto *scopes = root->find_by_id<Core::TabBar>("settings-scopes");
		REQUIRE(scopes != nullptr);
		REQUIRE(scopes->get_tab_count() == 2);
		CHECK(scopes->get_tab(1)->get_tooltip() == "Saved in this project, shipped with its builds");
		CHECK(root->find_by_id<Core::TextInput>("settings-search") != nullptr);
		CHECK(root->find_by_id("settings-nav") != nullptr);
		CHECK(root->find_by_id("settings-content") != nullptr);
		CHECK(root->find_by_id<Core::Label>("settings-footer-note") != nullptr);
		CHECK(root->find_by_id<Core::Button>("settings-close") != nullptr);

		Core::View parent;
		auto *nav = dynamic_cast<Core::Button *>(loader.create(
			file, "SettingsNavItem", parent, { .attributes = { { "label", "Tiles $1 & <b>" }, { "icon", "eye" } } }));
		REQUIRE(nav != nullptr);
		REQUIRE(nav->get_content() != nullptr);
		CHECK(nav->get_content()->get_label()->get_text() == "Tiles $1 & <b>");

		Core::View *section = loader.create(file, "SettingsSection", parent);
		REQUIRE(section != nullptr);
		CHECK(section->find_by_id<Core::Label>("title") != nullptr);
		CHECK(section->find_by_id<Core::Label>("description") != nullptr);
		const auto *grid = section->find_by_id<Core::PropertyGrid>("grid");
		REQUIRE(grid != nullptr);
		CHECK(grid->get_label_width() == 150.F);
		CHECK(grid->is_split());
		CHECK(section->find_by_id<Core::Button>("reset") != nullptr);

		Core::View *row = loader.create(file, "SettingsShortcutRow", parent);
		REQUIRE(row != nullptr);
		CHECK(row->find_by_id<Core::Label>("title") != nullptr);
		CHECK(row->find_by_id("chord") != nullptr);
		CHECK(row->find_by_id<Core::Button>("reset") != nullptr);

		using TextComponent = std::pair<const char *, const char *>;
		constexpr std::array<TextComponent, 3> K_TEXT_COMPONENTS = { {
			{ "SettingsShortcutGroup", "title" },
			{ "SettingsShortcutClash", "text" },
			{ "SettingsEmpty", "text" },
		} };
		for (const auto &[component, prop] : K_TEXT_COMPONENTS) {
			auto *label = dynamic_cast<Core::Label *>(
				loader.create(file, component, parent, { .attributes = { { prop, "Nothing $1 & <b>" } } }));
			REQUIRE_MESSAGE(label != nullptr, component);
			CHECK(label->get_text() == "Nothing $1 & <b>");
		}
		CHECK_MESSAGE(capture.stream.str().find(file + ":") == std::string::npos, capture.stream.str());
	}

	TEST_CASE("the launcher layout holds its header, rail, pages and components") {
		test_vfs();
		LogCapture capture;
		Core::LayoutLoader loader;
		const std::string file = "/resources/Engine/UI/Windows/launcher.aqlayout";
		const auto root = loader.load_file(file);
		REQUIRE(root != nullptr);
		CHECK(root->get_id() == "launcher-root");
		const auto *rail = root->find_by_id<Core::TabBar>("launcher-rail");
		REQUIRE(rail != nullptr);
		CHECK(rail->get_tab_count() == 3);
		constexpr std::array<const char *, 22> K_IDS = {
			"launcher-version",		   "launcher-page-recent",	 "launcher-page-new",	  "launcher-page-learn",
			"launcher-count",		   "launcher-grid-scroll",	 "launcher-grid",		  "launcher-empty",
			"launcher-empty-create",   "launcher-action-bar",	 "launcher-action-label", "launcher-action-input",
			"launcher-action-confirm", "launcher-action-cancel", "launcher-templates",	  "launcher-name",
			"launcher-location",	   "launcher-error",		 "launcher-create",		  "launcher-back",
			"launcher-wiki",		   "launcher-menu",
		};
		for (const char *id : K_IDS) {
			CHECK_MESSAGE(root->find_by_id(id) != nullptr, id);
		}
		CHECK(root->find_by_id<Core::PopupMenu>("launcher-menu") != nullptr);
		CHECK(root->find_by_id<Core::TextInput>("launcher-name")->get_placeholder() == "My project");

		Core::View parent;
		Core::View *row = loader.create(file, "LauncherRow", parent);
		REQUIRE(row != nullptr);
		CHECK(loader.create(file, "LauncherSpacer", *row) != nullptr);
		Core::ComponentArgs card_args{
			.attributes = { { "name", "Shader $1 & <b>" }, { "meta", "m" }, { "letter", "S" }, { "palette", "3" } }
		};
		auto *card = dynamic_cast<Core::Clickable *>(loader.create(file, "LauncherCard", *row, std::move(card_args)));
		REQUIRE(card != nullptr);
		bool named = false;
		bool themed = false;
		const auto find = [&](const auto &self, const Core::View &view) -> void {
			for (const auto &child : view.get_children()) {
				if (const auto *text = dynamic_cast<const Core::Label *>(child.get())) {
					named = named || text->get_text() == "Shader $1 & <b>";
				}
				const auto &classes = child->get_classes();
				themed = themed || std::ranges::find(classes, "launcher-thumb-3") != classes.end();
				self(self, *child);
			}
		};
		find(find, *card);
		CHECK(named);
		CHECK(themed);
		CHECK(dynamic_cast<Core::Clickable *>(
				  loader.create(file, "LauncherTemplate", parent,
								{ .attributes = { { "name", "Empty" }, { "description", "Nothing" } } })) != nullptr);
		CHECK_MESSAGE(capture.stream.str().find(file + ":") == std::string::npos, capture.stream.str());
	}

	TEST_CASE("the widget gallery layout holds every widget and its item component") {
		test_vfs();
		LogCapture capture;
		Core::LayoutLoader loader;
		register_editor_widget_stand_ins(loader);
		const std::string file = "/resources/Engine/UI/Windows/widget_gallery.aqlayout";
		const auto root = loader.load_file(file);
		REQUIRE(root != nullptr);
		CHECK(root->get_id() == "gallery-root");
		for (const char *id : { "gallery-grid", "gallery-tooltip", "gallery-tooltip-button" }) {
			CHECK_MESSAGE(root->find_by_id(id) != nullptr, id);
		}
		std::vector<std::string> headings;
		const auto collect = [&headings](const auto &self, const Core::View &view) -> void {
			for (const auto &child : view.get_children()) {
				const auto &classes = child->get_classes();
				if (std::ranges::find(classes, "gallery-heading") != classes.end()) {
					headings.push_back(dynamic_cast<const Core::Label &>(*child).get_text());
				}
				self(self, *child);
			}
		};
		collect(collect, *root);
		CHECK(headings.size() == 23);
		CHECK(headings.front() == "Button");
		CHECK(headings.back() == "Tooltip");

		Core::View parent;
		Core::View *item =
			loader.create(file, "GalleryItem", parent, { .attributes = { { "title", "Card $1 & <b>" } } });
		REQUIRE(item != nullptr);
		REQUIRE_FALSE(item->get_children().empty());
		CHECK(dynamic_cast<const Core::Label &>(*item->get_children().front()).get_text() == "Card $1 & <b>");
		CHECK_MESSAGE(capture.stream.str().find(file + ":") == std::string::npos, capture.stream.str());
	}

	TEST_CASE("the ui debug window layout holds its toolbar, panes, sections and box model") {
		test_vfs();
		LogCapture capture;
		Core::LayoutLoader loader;
		const std::string file = "/resources/Engine/UI/Windows/ui_debug.aqlayout";
		const auto root = loader.load_file(file);
		REQUIRE(root != nullptr);
		CHECK(root->get_id() == "inspector-root");
		constexpr std::array<const char *, 15> K_PARTS = {
			"inspector-pick",  "inspector-refresh", "inspector-filter",	  "inspector-tree",
			"inspector-empty", "inspector-detail",	"inspector-subtitle", "inspector-breadcrumb",
			"bm-top",		   "bm-right",			"bm-bottom",		  "bm-left",
			"bm-content",	   "section-element",	"section-text",
		};
		for (const char *id : K_PARTS) {
			CHECK_MESSAGE(root->find_by_id(id) != nullptr, id);
		}
		CHECK(root->find_by_id<Core::TreeView>("inspector-tree") != nullptr);
		constexpr std::array<const char *, 34> K_VALUES = {
			"v-type",	"v-id",		   "v-classes",	   "v-state",	 "v-children", "v-pos",		"v-size",
			"v-ids",	"v-display",   "v-position",   "v-overflow", "v-flex",	   "v-justify", "v-align",
			"v-grow",	"v-gap",	   "v-dims",	   "v-minmax",	 "v-aspect",   "v-inset",	"v-zindex",
			"v-bg",		"v-color",	   "v-border",	   "v-radius",	 "v-opacity",  "v-shadows", "v-fontsize",
			"v-family", "v-textalign", "v-whitespace", "sw-bg",		 "sw-color",   "sw-border",
		};
		for (const char *id : K_VALUES) {
			CHECK_MESSAGE(root->find_by_id(id) != nullptr, id);
		}
		CHECK_MESSAGE(capture.stream.str().find(file + ":") == std::string::npos, capture.stream.str());
	}

	TEST_CASE("the ui debug panel component holds its window, refresh button and tree") {
		test_vfs();
		LogCapture capture;
		Core::LayoutLoader loader;
		Core::View parent;
		Core::View *panel = loader.create("Components/ui_debug_panel", "UIDebugPanel", parent);
		REQUIRE(panel != nullptr);
		CHECK(panel->get_id() == "ui-debug-window");
		REQUIRE(panel->has_floating());
		CHECK(panel->get_floating().z_index == 1000);
		CHECK(panel->find_by_id<Core::Button>("refresh") != nullptr);
		CHECK(panel->find_by_id<Core::TreeView>("tree") != nullptr);
		CHECK_MESSAGE(capture.stream.str().find("ui_debug_panel.aqlayout:") == std::string::npos, capture.stream.str());
	}

	TEST_CASE("the editor shell layout holds its top bar, dock space, status bar and main menu") {
		test_vfs();
		LogCapture capture;
		Core::LayoutLoader loader;
		Core::Commands commands;
		for (const char *id :
			 { "scene.new", "scene.open", "scene.save", "scene.save_as", "editor.quit", "editor.preferences",
			   "play.start", "play.pause", "play.stop", "scene.reset_demo", "window.open.build", "workspace.reset",
			   "view.status_bar", "devtools.ui_inspector", "devtools.widget_gallery" }) {
			commands.add({ .id = id, .title = id });
		}
		loader.set_command_source(&commands);
		const std::string file = "/resources/Engine/UI/Windows/editor.aqlayout";
		const auto root = loader.load_file(file);
		REQUIRE(root != nullptr);
		CHECK(root->get_id() == "root");
		for (const char *id :
			 { "topbar", "topbar-menu", "workspace-tabs", "topbar-play", "topbar-pause", "topbar-stop", "topbar-build",
			   "topbar-reset", "topbar-settings", "dock-root", "editor-dock", "hud-status", "status-selection",
			   "stat-objects", "stat-lights", "stat-vertices", "stat-triangles", "window-menu" }) {
			CHECK_MESSAGE(root->find_by_id(id) != nullptr, id);
		}
		auto *menu = root->find_by_id<Core::PopupMenu>("main-menu");
		REQUIRE(menu != nullptr);
		int submenus = 0;
		for (const auto &child : menu->get_children()) {
			submenus += dynamic_cast<const Core::PopupMenu *>(child.get()) != nullptr ? 1 : 0;
		}
		CHECK(submenus == 3);

		const auto rows = [](Core::PopupMenu &target) {
			target.refresh();
			std::vector<std::string> texts;
			for (const auto &child : target.get_children()) {
				const auto &classes = child->get_classes();
				if (std::ranges::find(classes, "menu-separator") != classes.end()) {
					texts.emplace_back("---");
				} else if (const auto *item = dynamic_cast<const Core::Button *>(child.get());
						   item != nullptr && std::ranges::find(classes, "menu-item") != classes.end()) {
					texts.push_back(item->get_content()->get_label()->get_text());
				}
			}
			return texts;
		};
		std::vector<Core::PopupMenu *> submenu_list;
		for (const auto &child : menu->get_children()) {
			if (auto *submenu = dynamic_cast<Core::PopupMenu *>(child.get())) {
				submenu_list.push_back(submenu);
			}
		}
		REQUIRE(submenu_list.size() == 3);
		CHECK(rows(*menu) == std::vector<std::string>{ "File", "Edit", "Window" });
		CHECK(
			rows(*submenu_list[0]) ==
			std::vector<std::string>{ "scene.new", "scene.open", "scene.save", "scene.save_as", "---", "editor.quit" });
		CHECK(rows(*submenu_list[1]) ==
			  std::vector<std::string>{ "editor.preferences", "---", "play.start", "play.pause", "play.stop", "---",
										"scene.reset_demo" });
		auto *window_menu = root->find_by_id<Core::PopupMenu>("window-menu");
		REQUIRE(window_menu == submenu_list[2]);
		CHECK(rows(*window_menu) ==
			  std::vector<std::string>{ "window.open.build", "---", "workspace.reset", "view.status_bar", "---",
										"devtools.ui_inspector", "devtools.widget_gallery" });
		CHECK_MESSAGE(capture.stream.str().find(file + ":") == std::string::npos, capture.stream.str());
		CHECK_FALSE(std::filesystem::exists(K_UI_DIR / "editor.aqlayout"));
	}
}
