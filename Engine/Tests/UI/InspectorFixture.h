#ifndef AQUILA_TESTS_UI_INSPECTOR_FIXTURE_H
#define AQUILA_TESTS_UI_INSPECTOR_FIXTURE_H

#include "Aquila/Foundation/Reflection/TypeInfo.h"
#include "Aquila/UI/Core/LayoutLoader.h"
#include "Aquila/UI/Core/UIResources.h"
#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/Collapsible.h"
#include "Aquila/UI/Widgets/Dropdown.h"
#include "Aquila/UI/Widgets/InspectorPage.h"
#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/PropertyGrid.h"
#include "Aquila/UI/Widgets/ReflectedPropertyGrid.h"
#include "Aquila/UI/Widgets/TextInput.h"

namespace InspectorFixture {

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
	std::string texture = "/project/Content/Materials_Tiles_03/JPEG/Tiles_GreenAndGoldPattern_01/"
						  "Tiles_GreenAndGoldPattern_01_basecolor.jpg";
};

inline constexpr std::array<Aquila::Reflection::PropertyMark, 3> K_MARKS = { {
	{ .value = 0.25F, .label = "Full moon" },
	{ .value = 2000.F, .label = "Overcast day" },
	{ .value = 100000.F, .label = "Clear noon sun" },
} };

inline const std::array<Vec4, 2> K_GRADIENT = { Vec4(1.F, 0.5F, 0.F, 1.F), Vec4(0.5F, 0.7F, 1.F, 1.F) };

inline const Aquila::Reflection::TypeInfo &everything_type() {
	using Aquila::Reflection::TypeBuilder;
	using Aquila::Reflection::TypeInfo;
	static const TypeInfo type = [] {
		TypeInfo info("Everything");
		TypeBuilder<Everything>(info)
			.property("Distance From The Camera", &Everything::distance,
					  { .speed = 0.05F, .precision = 3, .unit = "m" })
			.property("Roughness", &Everything::roughness, { .min = 0.F, .max = 1.F, .slider = true })
			.property("Intensity", &Everything::illuminance,
					  { .min = 0.F,
						.max = 1000000.F,
						.precision = 2,
						.unit = "lx",
						.logarithmic = true,
						.slider_min = 0.01F,
						.slider_max = 150000.F,
						.marks = K_MARKS })
			.property("Temperature", &Everything::temperature,
					  { .min = 1667.F, .max = 12000.F, .precision = 0, .unit = "K", .gradient = K_GRADIENT })
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

struct EverythingInspector {
	Everything instance;
	Unique<Aquila::UI::Core::ReflectedPropertyGrid> reflected;

	void build(Aquila::UI::Core::LayoutLoader &loader, Aquila::UI::Core::View &box,
			   const std::string &section_title = "Directional Light With A Long Component Name") {
		using namespace Aquila::UI::Core;
		Unique<View> layout = loader.load_file("/resources/Engine/UI/inspector.aqlayout");
		REQUIRE(layout != nullptr);
		View *root = box.add_child(std::move(layout));

		const std::string components = engine_component_file("inspector");
		for (int i = 0; i < 3; ++i) {
			REQUIRE(loader.create(components, "InspectorTabButton", *root->find_by_id("props-tabs")) != nullptr);
		}
		root->find_by_id("inspector-empty")->set_hidden(true);
		root->find_by_id<TextInput>("inspector-name")->set_text(instance.name);
		root->find_by_id<Label>("inspector-uuid")->set_text("550e8400-e29b-41d4-a716-446655440000");

		auto *page = loader.create<InspectorPage>(components, "InspectorPage", *root->find_by_id("inspector-pages"));
		REQUIRE(page != nullptr);
		auto *section = dynamic_cast<Collapsible *>(loader.create(components, "InspectorPanel", page->get_sections(),
																  { .attributes = { { "title", section_title } } }));
		REQUIRE(section != nullptr);
		section->add_header_action(nullptr, "Remove", [] {});
		auto *grid = section->add_child<PropertyGrid>();
		grid->set_split(true);
		reflected = std::make_unique<ReflectedPropertyGrid>(*grid, everything_type(), nullptr);
		reflected->bind(&instance);
		style_dropdowns(*section);

		auto *add = dynamic_cast<Button *>(loader.create(components, "InspectorAction", page->root()));
		REQUIRE(add != nullptr);
		add->set_text("Add Component");

		auto *path = page->root().add_child<Label>(instance.texture);
		path->add_class("props-uuid");
	}

	static void style_dropdowns(Aquila::UI::Core::View &view) {
		if (auto *dropdown = dynamic_cast<Aquila::UI::Core::Dropdown *>(&view)) {
			dropdown->set_variant("props-dropdown");
		}
		for (const auto &child : view.get_children()) {
			style_dropdowns(*child);
		}
	}
};

}

#endif
