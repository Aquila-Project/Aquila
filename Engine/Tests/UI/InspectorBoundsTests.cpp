#include <doctest.h>

#include "Aquila/Foundation/Reflection/TypeInfo.h"
#include "Aquila/UI/Core/Canvas.h"
#include "Aquila/UI/Core/LayoutLoader.h"
#include "Aquila/UI/Style/StyleParser.h"
#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/Collapsible.h"
#include "Aquila/UI/Widgets/Dropdown.h"
#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/PropertyGrid.h"
#include "Aquila/UI/Widgets/ReflectedPropertyGrid.h"
#include "Aquila/UI/Widgets/TextInput.h"

#include "CanvasTestSupport.h"
#include "GpuTestSupport.h"

using namespace Aquila;
using namespace Aquila::UI;
using namespace Aquila::UI::Core;
using Aquila::Reflection::TypeBuilder;
using Aquila::Reflection::TypeInfo;

namespace {

enum class Mode { Directional, Point, Spot };

struct Everything {
	F32 distance = 2.5F;
	F32 roughness = 0.5F;
	F32 illuminance = 100000.F;
	F32 temperature = 6500.F;
	Int32 count = 3;
	bool visible = true;
	bool shadows = false;
	Vec2 tiling = Vec2(1.F);
	Vec3 position = Vec3(1.F, 2.F, 3.F);
	Vec3 tint = Vec3(1.F);
	Vec4 base_color = Vec4(1.F);
	Mode mode = Mode::Spot;
	std::string name = "Tiles_GreenAndGoldPattern_01_basecolor_material";
	std::string texture =
		"/project/Content/Materials_Tiles_03/JPEG/Tiles_GreenAndGoldPattern_01/Tiles_GreenAndGoldPattern_01_basecolor.jpg";
};

const TypeInfo &everything_type() {
	static const TypeInfo type = [] {
		TypeInfo info("Everything");
		TypeBuilder<Everything>(info)
			.property("Distance From The Camera", &Everything::distance, { .speed = 0.05F, .precision = 3, .unit = "m" })
			.property("Roughness", &Everything::roughness, { .min = 0.F, .max = 1.F, .slider = true })
			.property("Count", &Everything::count)
			.property("Visible", &Everything::visible)
			.property("Cast Shadows", &Everything::shadows, { .toggle = true })
			.property("Tiling", &Everything::tiling)
			.property("Position", &Everything::position, { .speed = 0.05F, .precision = 3, .unit = "m" })
			.property("Tint", &Everything::tint, { .color = true })
			.property("Base Color", &Everything::base_color, { .color = true })
			.property("Mode", &Everything::mode)
			.options({ { .name = "Directional", .value = 0 },
					   { .name = "Point", .value = 1 },
					   { .name = "Spot light with a long name", .value = 2 } })
			.property("Name", &Everything::name)
			.property("Base Color Map", &Everything::texture, { .asset_type = "texture" })
			.read_only("UUID", [](const Everything &) { return std::string("550e8400-e29b-41d4-a716-446655440000"); });
		return info;
	}();
	return type;
}

struct Inspector {
	CanvasSingletonsScope singletons;
	Canvas canvas{ 1200, 900 };
	View *box = nullptr;
	Everything instance;
	Unique<ReflectedPropertyGrid> reflected;

	explicit Inspector(F32 width) {
		test_gpu();
		const std::array<std::string, 3> styles = { "/resources/Engine/UI/tokens.aqstyle",
													"/resources/Engine/UI/widgets.aqstyle",
													"/resources/Engine/UI/editor_chrome.aqstyle" };
		StyleParser::load_files(styles, canvas.get_style_sheet());

		box = canvas.get_root()->add_child<View>();
		StyleProperties style;
		style.width = StyleLength::pixel(width);
		style.height = StyleLength::pixel(880.F);
		style.flex_direction = FlexDirection::Column;
		style.overflow = Overflow::Hidden;
		style.font_family = "regular";
		box->merge_style(style);

		LayoutLoader loader;
		Unique<View> layout = loader.load_file("/resources/Engine/UI/inspector.aqlayout");
		REQUIRE(layout != nullptr);
		View *root = box->add_child(std::move(layout));

		for (int i = 0; i < 3; ++i) {
			root->find_by_id("props-tabs")->add_child<Button>()->add_class("props-tab");
		}
		root->find_by_id("inspector-empty")->set_hidden(true);
		root->find_by_id<TextInput>("inspector-name")->set_text(instance.name);
		root->find_by_id<Label>("inspector-uuid")->set_text("550e8400-e29b-41d4-a716-446655440000");

		auto *page = root->find_by_id("inspector-pages")->add_child<View>();
		page->add_class("props-page");
		auto *sections = page->add_child<View>();
		sections->add_class("props-sections");
		auto *section = sections->add_child<Collapsible>("Directional Light With A Long Component Name");
		section->set_variant("props-panel");
		section->add_header_action(nullptr, "Remove", [] {});
		auto *grid = section->add_child<PropertyGrid>();
		grid->set_split(true);
		reflected = std::make_unique<ReflectedPropertyGrid>(*grid, everything_type(), nullptr);
		reflected->bind(&instance);
		style_dropdowns(*section);

		auto *add = page->add_child<Button>();
		add->add_class("props-add-component");
		add->set_text("Add Component");

		auto *path = page->add_child<Label>(instance.texture);
		path->add_class("asset-inspector-path");
	}

	static void style_dropdowns(View &view) {
		if (auto *dropdown = dynamic_cast<Dropdown *>(&view)) {
			dropdown->set_variant("props-dropdown");
		}
		for (const auto &child : view.get_children()) {
			style_dropdowns(*child);
		}
	}

	void layout() {
		canvas.reload_styles();
		for (int i = 0; i < 4; ++i) {
			canvas.update(0.016F);
			canvas.compute();
		}
	}

	[[nodiscard]] std::vector<std::string> overflowing(bool visible_only) const {
		std::vector<std::string> found;
		const Rect bounds = box->get_absolute_rect();
		collect(*box, bounds, visible_only, found);
		return found;
	}

	static void collect(const View &view, const Rect &bounds, bool visible_only, std::vector<std::string> &found) {
		for (const auto &child : view.get_children()) {
			const ComputedStyle &style = child->get_display_style();
			if (style.display == Display::None) {
				continue;
			}
			const Rect rect = child->get_absolute_rect();
			if (rect.size.x > 0.F && rect.size.y > 0.F &&
				(rect.left() < bounds.left() - 0.5F || rect.right() > bounds.right() + 0.5F)) {
				std::string classes;
				for (const std::string &name : child->get_classes()) {
					classes += "." + name;
				}
				found.push_back(std::string(child->get_type_name()) + classes + " x " + std::to_string(rect.left()) +
								".." + std::to_string(rect.right()));
			}
			const bool sizer = std::ranges::find(child->get_classes(), "dropdown-sizer") != child->get_classes().end();
			const bool clips = style.overflow == Overflow::Clip || style.overflow == Overflow::Hidden;
			if (!sizer && !(visible_only && clips)) {
				collect(*child, bounds, visible_only, found);
			}
		}
	}
};

}

TEST_SUITE("Inspector bounds") {
	TEST_CASE("every inspector row fits a narrow panel") {
		for (const F32 width : { 140.F, 180.F, 220.F, 280.F, 360.F }) {
			CAPTURE(width);
			Inspector inspector(width);
			inspector.layout();
			const std::vector<std::string> overflowing = inspector.overflowing(false);
			for (const std::string &view : overflowing) {
				MESSAGE(width << "px: " << view);
			}
			CHECK(overflowing.empty());
		}
	}

	TEST_CASE("nothing draws outside a panel too narrow to fit the rows") {
		for (const F32 width : { 60.F, 100.F }) {
			CAPTURE(width);
			Inspector inspector(width);
			inspector.layout();
			const std::vector<std::string> overflowing = inspector.overflowing(true);
			for (const std::string &view : overflowing) {
				MESSAGE(width << "px: " << view);
			}
			CHECK(overflowing.empty());
		}
	}
}
