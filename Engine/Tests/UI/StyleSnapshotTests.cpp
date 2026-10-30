#include <doctest.h>

#include "Aquila/UI/Core/Behaviour.h"
#include "Aquila/UI/Core/UIResources.h"
#include "Aquila/UI/Widgets/Checkbox.h"
#include "Aquila/UI/Widgets/Clickable.h"
#include "Aquila/UI/Widgets/DockNode.h"
#include "Aquila/UI/Widgets/DockPanel.h"
#include "Aquila/UI/Widgets/DockSpace.h"
#include "Aquila/UI/Widgets/DragInt.h"
#include "Aquila/UI/Widgets/Dropdown.h"
#include "Aquila/UI/Widgets/Image.h"
#include "Aquila/UI/Widgets/InspectorView.h"
#include "Aquila/UI/Widgets/PropertyGrid.h"
#include "Aquila/UI/Widgets/SelectableTextView.h"
#include "Aquila/UI/Widgets/DragFloat.h"
#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/OverlayCheckRow.h"
#include "Aquila/UI/Widgets/Popup.h"
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
		auto group_node = std::make_unique<TreeNode>("Geometry", *tree, 0);
		group_node->set_selectable(false);
		group_node->get_header()->add_class("hierarchy-group-header");
		group_node->set_row_component(file, "HierarchyGroupCount");
		auto *group = dynamic_cast<TreeNode *>(tree->add_child(std::move(group_node)));
		REQUIRE(group != nullptr);
		auto *count = dynamic_cast<Label *>(group->get_row_component());
		REQUIRE(count != nullptr);
		count->set_text("3");
		TreeNode *parent = group->add_child_node("Cube");
		eye_of(*parent, false);
		eye_of(*parent->add_child_node("Child"), false);
		eye_of(*group->add_child_node("Hidden"), true);
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
}
