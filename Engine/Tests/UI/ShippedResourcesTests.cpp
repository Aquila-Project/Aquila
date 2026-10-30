#include <doctest.h>

#include "Aquila/UI/Core/Behaviour.h"
#include "Aquila/UI/Core/LayoutLoader.h"
#include "Aquila/UI/Core/UIResources.h"
#include "Aquila/UI/Core/View.h"
#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/Image.h"
#include "Aquila/UI/Widgets/Clickable.h"
#include "Aquila/UI/Widgets/Checkbox.h"
#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/OverlayCheckRow.h"
#include "Aquila/UI/Widgets/Popup.h"
#include "Aquila/UI/Widgets/PropertyGrid.h"
#include "Aquila/UI/Widgets/TextInput.h"
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
	for (const char *widget : { "ColorPicker", "RenameField", "TransformGizmo", "ViewportOverlay" }) {
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
			"/resources/Engine/UI/editor_chrome.aqstyle",
			"/resources/Engine/UI/launcher.aqstyle",
			"/resources/Engine/UI/devtools.aqstyle",
			"/resources/Engine/UI/Windows/console.aqstyle",
			"/resources/Engine/UI/Windows/build.aqstyle",
		};
		REQUIRE(Aquila::UI::StyleParser::load_files(paths, sheet));
		const std::vector<std::pair<std::string, Vec4>> expected{
			{ "console-tag-info", { 0.35F, 0.87F, 0.35F, 1.F } },
			{ "console-tag-debug", { 0.F, 0.8F, 0.8F, 1.F } },
			{ "console-tag-trace", { 0.55F, 0.55F, 0.55F, 1.F } },
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
		CHECK(dynamic_cast<Core::Label *>(loader.create(file, "HierarchyGroupCount", parent)) != nullptr);
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
}
