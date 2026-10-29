#include <doctest.h>

#include "Aquila/UI/Core/Behaviour.h"
#include "Aquila/UI/Core/UIResources.h"
#include "Aquila/UI/Widgets/Checkbox.h"
#include "Aquila/UI/Widgets/DockNode.h"
#include "Aquila/UI/Widgets/DockPanel.h"
#include "Aquila/UI/Widgets/DockSpace.h"
#include "Aquila/UI/Widgets/DragInt.h"
#include "Aquila/UI/Widgets/Dropdown.h"
#include "Aquila/UI/Widgets/InspectorView.h"
#include "Aquila/UI/Widgets/PropertyGrid.h"
#include "Aquila/UI/Widgets/TextInput.h"
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
			View *root = fixture.loader.build_file("/resources/Engine/UI/inspector.aqlayout", box);
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

	TEST_CASE("style snapshot console" * doctest::skip()) {
		LoaderCanvas fixture(1200, 900);
		View &box = snapshot_box(fixture, 640.F, 400.F);
		REQUIRE(fixture.loader.build_file("/resources/Engine/UI/Windows/console.aqlayout", box) != nullptr);
		fixture.settle_fully();
		write_style_snapshot("console", box);
	}

	TEST_CASE("style snapshot build" * doctest::skip()) {
		LoaderCanvas fixture(1200, 900);
		View &box = snapshot_box(fixture, 720.F, 600.F);
		View *window = fixture.loader.build_file("/resources/Engine/UI/Windows/build.aqlayout", box);
		REQUIRE(window != nullptr);
		View *settings = window->find_by_id("build-settings");
		REQUIRE(settings != nullptr);
		auto *grid = settings->add_child<PropertyGrid>(120.F);
		grid->add_row<TextInput>("Game Name");
		grid->add_row<Dropdown>("Startup Scene");
		grid->add_row<DragInt>("Width");
		grid->add_check_row<Checkbox>("Fullscreen", false);
		grid->add_separator();
		grid->add_stacked_row<TextInput>("Output Folder");
		fixture.settle_fully();
		write_style_snapshot("build", box);
	}
}
