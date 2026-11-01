#include <doctest.h>

#include "Aquila/UI/Core/Canvas.h"
#include "Aquila/UI/Rendering/DrawList.h"
#include "Aquila/UI/Style/StyleParser.h"
#include "Aquila/UI/Widgets/DragFloat.h"

#include "CanvasTestSupport.h"
#include "GpuTestSupport.h"

using namespace Aquila;
using namespace Aquila::UI;
using namespace Aquila::UI::Core;

namespace {

constexpr Vec4 K_BACKGROUND = Vec4(0.125F, 0.125F, 0.125F, 1.F);
constexpr Vec4 K_ACCENT = Vec4(0.F, 1.F, 0.F, 1.F);

struct Fixture {
	CanvasSingletonsScope singletons;
	Canvas canvas{ 400, 200 };
	DragFloat *field = nullptr;

	explicit Fixture(std::string_view text_align) {
		test_gpu();
		const std::string css =
			std::format("DragFloat {{ width: 200px; height: 20px; padding: 0px 6px; gap: 5px; font-family: regular; "
						"font-size: 12px; color: #ffffff; background-color: #202020; accent-color: #00ff00; "
						"text-align: {}; }}",
						text_align);
		StyleParser::LoadString(css, canvas.get_style_sheet());
		field = canvas.get_root()->add_child<DragFloat>();
	}

	[[nodiscard]] std::vector<Rendering::DrawCmd> draw() {
		canvas.reload_styles();
		for (int i = 0; i < 4; ++i) {
			canvas.update(0.016F);
			canvas.compute();
		}
		Rendering::DrawList list;
		field->on_draw_self(list);
		list.sort();
		return list.take_commands();
	}
};

bool same_color(Vec4 a, Vec4 b) {
	return glm::all(glm::epsilonEqual(a, b, 0.01F));
}

std::vector<const Rendering::TextCmd *> texts(const std::vector<Rendering::DrawCmd> &commands) {
	std::vector<const Rendering::TextCmd *> found;
	for (const Rendering::DrawCmd &command : commands) {
		if (const auto *text = std::get_if<Rendering::TextCmd>(&command)) {
			found.push_back(text);
		}
	}
	return found;
}

}

TEST_SUITE("DragFloat") {
	TEST_CASE("a slider field draws its fill over its background") {
		Fixture fixture("center");
		fixture.field->set_range(0.F, 1.F);
		fixture.field->set_slider(true);
		fixture.field->set_value(0.5F);
		const std::vector<Rendering::DrawCmd> commands = fixture.draw();

		Option<Usize> background;
		Option<Usize> fill;
		for (Usize i = 0; i < commands.size(); ++i) {
			if (const auto *rect = std::get_if<Rendering::RectCmd>(&commands[i])) {
				if (same_color(rect->color, K_BACKGROUND)) {
					background = i;
				} else if (same_color(rect->color, K_ACCENT)) {
					fill = i;
				}
			}
		}
		REQUIRE(background.has_value());
		REQUIRE(fill.has_value());
		CHECK(*fill > *background);
	}

	TEST_CASE("the value sits where text-align puts it, inside the padding") {
		for (const auto &[css, align] :
			 { std::pair{ "left", TextAlign::Left }, std::pair{ "center", TextAlign::Center },
			   std::pair{ "right", TextAlign::Right } }) {
			CAPTURE(css);
			Fixture fixture(css);
			fixture.field->set_value(0.5F);
			const std::vector<Rendering::DrawCmd> commands = fixture.draw();
			const std::vector<const Rendering::TextCmd *> found = texts(commands);
			REQUIRE(found.size() == 1);
			CHECK(found[0]->align == align);
			const Rect field_rect = fixture.field->get_absolute_rect();
			CHECK(found[0]->rect.left() == doctest::Approx(field_rect.left() + 6.F));
			CHECK(found[0]->rect.right() == doctest::Approx(field_rect.right() - 6.F));
		}
	}

	TEST_CASE("with an inline label the value follows it unless it is right aligned") {
		{
			Fixture left("left");
			left.field->set_label("X");
			left.field->set_value(0.5F);
			const std::vector<Rendering::DrawCmd> commands = left.draw();
			const std::vector<const Rendering::TextCmd *> found = texts(commands);
			REQUIRE(found.size() == 2);
			CHECK(found[0]->text == "X");
			CHECK(found[1]->align == TextAlign::Left);
			const F32 label_width = test_gpu().font->measure_text("X", found[0]->font_size).x;
			CHECK(found[1]->rect.left() == doctest::Approx(found[0]->rect.left() + label_width + 5.F));
		}
		{
			Fixture right("right");
			right.field->set_label("X");
			right.field->set_value(0.5F);
			const std::vector<Rendering::DrawCmd> commands = right.draw();
			const std::vector<const Rendering::TextCmd *> found = texts(commands);
			REQUIRE(found.size() == 2);
			CHECK(found[1]->align == TextAlign::Right);
			CHECK(found[1]->rect.right() == doctest::Approx(right.field->get_absolute_rect().right() - 6.F));
		}
	}
}
