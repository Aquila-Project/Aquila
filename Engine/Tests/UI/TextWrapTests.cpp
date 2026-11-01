#include <doctest.h>

#include "Aquila/UI/Core/Canvas.h"
#include "Aquila/UI/Core/FontRegistry.h"
#include "Aquila/UI/Core/TextureCache.h"
#include "Aquila/UI/Style/StyleParser.h"
#include "Aquila/UI/Widgets/AssetSlot.h"
#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/DragFloat.h"
#include "Aquila/UI/Widgets/Dropdown.h"
#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/ScrollView.h"
#include "Aquila/UI/Widgets/TextInput.h"
#include "Aquila/UI/Widgets/TreeView.h"
#include "Aquila/UI/Rendering/DrawList.h"

#include "CanvasTestSupport.h"
#include "GpuTestSupport.h"

using namespace Aquila;
using namespace Aquila::UI;
using namespace Aquila::UI::Core;

namespace {

struct Fixture {
	CanvasSingletonsScope singletons;
	Canvas canvas{ 800, 600 };
	View *box = nullptr;

	explicit Fixture(F32 width, Overflow overflow = Overflow::Hidden, bool dock_styles = false) {
		test_gpu();
		std::vector<std::string> styles = { "/resources/Engine/UI/tokens.aqstyle",
											"/resources/Engine/UI/widgets.aqstyle" };
		if (dock_styles) {
			styles.emplace_back("/resources/Engine/UI/dock.aqstyle");
		}
		StyleParser::load_files(styles, canvas.get_style_sheet());

		box = canvas.get_root()->add_child<View>();
		StyleProperties style;
		style.width = StyleLength::pixel(width);
		style.flex_direction = FlexDirection::Column;
		style.overflow = overflow;
		style.font_family = "regular";
		box->merge_style(style);
	}

	void layout() {
		canvas.reload_styles();
		for (int i = 0; i < 4; ++i) {
			canvas.update(0.016F);
			canvas.compute();
		}
	}
};

constexpr const char *K_LONG_TEXT = "Build and run the whole project with every scene in it";
constexpr const char *K_FILE_NAME = "Tiles_GreenAndGoldPattern_01_basecolor.jpg";

Label *one_line_label(View &parent, TextOverflow overflow) {
	auto *label = parent.add_child<Label>(std::string(K_FILE_NAME));
	StyleProperties style;
	style.width = StyleLength::grow();
	style.min_width = StyleLength::pixel(0.F);
	style.white_space = WhiteSpace::Nowrap;
	style.text_overflow = overflow;
	label->merge_style(style);
	return label;
}

std::string drawn_text(View &view) {
	Rendering::DrawList list;
	view.on_draw_self(list);
	for (const Rendering::DrawCmd &command : list.take_commands()) {
		if (const auto *text = std::get_if<Rendering::TextCmd>(&command)) {
			return text->text;
		}
	}
	return {};
}

void collect_labels(const View &view, std::vector<const Label *> &found) {
	if (const auto *label = dynamic_cast<const Label *>(&view); label != nullptr && !label->get_text().empty()) {
		found.push_back(label);
	}
	for (const auto &child : view.get_children()) {
		collect_labels(*child, found);
	}
}

void check_one_line(const View &view) {
	std::vector<const Label *> labels;
	collect_labels(view, labels);
	REQUIRE(!labels.empty());
	for (const Label *label : labels) {
		CAPTURE(label->get_text());
		const F32 line = test_gpu().font->get_line_height() * label->get_display_style().font_size;
		CHECK(label->get_layout_rect().size.y < line * 1.5F);
		CHECK(label->get_display_style().white_space == WhiteSpace::Nowrap);
	}
}

bool has_ellipsis(const std::string &text) {
	return text.find("\u2026") != std::string::npos || text.find("...") != std::string::npos;
}

F32 single_line_height(const std::function<View *(View &)> &make) {
	Fixture wide(2000.F);
	View *view = make(*wide.box);
	wide.layout();
	return view->get_layout_rect().size.y;
}

}

TEST_SUITE("Text wrapping") {
	TEST_CASE("a label wraps inside a narrow clipped column") {
		Fixture narrow(120.F);
		auto *label = narrow.box->add_child<Label>(std::string(K_LONG_TEXT));
		narrow.layout();
		CHECK(label->get_layout_rect().size.x <= 120.F + 0.5F);
		CHECK(label->get_layout_rect().size.y > test_gpu().font->measure_text("A", 14.F).y * 1.5F);
	}

	TEST_CASE("a button grows taller when its text wraps") {
		auto make = [](View &parent) -> View * { return parent.add_child<Button>(std::string(K_LONG_TEXT)); };
		const F32 one_line = single_line_height(make);

		Fixture narrow(120.F);
		View *button = make(*narrow.box);
		narrow.layout();
		CHECK(button->get_layout_rect().size.x <= 120.F + 0.5F);
		CHECK(button->get_layout_rect().size.y > one_line * 1.5F);
	}

	TEST_CASE("a dropdown follows its wrapped header text") {
		auto make = [](View &parent) -> View * {
			auto *dropdown = parent.add_child<Dropdown>();
			dropdown->add_option("short", "Short");
			dropdown->add_option("long", K_LONG_TEXT);
			dropdown->set_value("long");
			return dropdown;
		};
		const F32 one_line = single_line_height(make);

		Fixture narrow(140.F);
		auto *dropdown = static_cast<Dropdown *>(make(*narrow.box));
		narrow.layout();
		const View &header = *dropdown->get_children().front();
		CHECK(dropdown->get_layout_rect().size.x <= 140.F + 0.5F);
		CHECK(dropdown->get_layout_rect().size.y > one_line * 1.5F);
		CHECK(header.get_layout_rect().size.y == doctest::Approx(dropdown->get_layout_rect().size.y));
	}

	TEST_CASE("a dropdown keeps the width of its widest option when there is room") {
		Fixture wide(2000.F);
		StyleProperties row;
		row.flex_direction = FlexDirection::Row;
		row.align_items = AlignItems::Start;
		wide.box->merge_style(row);
		auto *dropdown = wide.box->add_child<Dropdown>();
		dropdown->add_option("short", "Short");
		dropdown->add_option("long", K_LONG_TEXT);
		dropdown->set_value("short");
		wide.layout();
		const F32 size = dropdown->get_children().front()->get_display_style().font_size;
		CHECK(dropdown->get_layout_rect().size.x > test_gpu().font->measure_text(K_LONG_TEXT, size).x);
		CHECK(dropdown->get_layout_rect().size.y < 2.F * test_gpu().font->measure_text("A", size).y + 12.F);
	}

	TEST_CASE("a long option without spaces doesn't stop a dropdown from shrinking") {
		Fixture narrow(140.F);
		auto *dropdown = narrow.box->add_child<Dropdown>();
		dropdown->add_option("default", "Aquila logo");
		dropdown->add_option("file", "a_very_long_file_name_without_a_single_space_in_it.png");
		dropdown->set_value("default");
		narrow.layout();
		CHECK(dropdown->get_layout_rect().size.x <= 140.F + 0.5F);
	}

	TEST_CASE("a text input clips its text to its box") {
		Fixture narrow(120.F);
		auto *input = narrow.box->add_child<TextInput>(std::string("Folder"));
		input->set_text("/home/someone/a/long/path/that/never/fits/in/the/field");
		narrow.layout();
		CHECK(input->get_layout_rect().size.x <= 120.F + 0.5F);
	}

	TEST_CASE("dropdown content sits in the middle of the header") {
		for (const char *variant : { "", "props-dropdown", "overlay-dropdown" }) {
			CAPTURE(variant);
			Fixture wide(400.F);
			StyleProperties row;
			row.flex_direction = FlexDirection::Row;
			row.align_items = AlignItems::Start;
			wide.box->merge_style(row);
			auto *dropdown = wide.box->add_child<Dropdown>();
			if (*variant != '\0') {
				dropdown->set_variant(variant);
			}
			dropdown->add_option("lit", "Lit");
			dropdown->add_option("albedo", "Albedo");
			dropdown->set_value("lit");
			wide.layout();

			const View &header = *dropdown->get_children().front();
			const View &content = *header.get_children().front();
			const F32 header_middle = header.get_absolute_position().y + (header.get_layout_rect().size.y * 0.5F);
			const F32 content_middle = content.get_absolute_position().y + (content.get_layout_rect().size.y * 0.5F);
			CHECK(content_middle == doctest::Approx(header_middle).epsilon(0.01));
			CHECK(header.get_layout_rect().size.y == doctest::Approx(dropdown->get_layout_rect().size.y));
		}
	}

	TEST_CASE("text in a scroll view wraps to the view width") {
		Fixture narrow(150.F, Overflow::Visible);
		StyleProperties fixed_height;
		fixed_height.height = StyleLength::pixel(200.F);
		auto *scroll = narrow.box->add_child<ScrollView>();
		scroll->merge_style(fixed_height);
		auto *label = scroll->add_child<Label>(std::string(K_LONG_TEXT));
		narrow.layout();
		CHECK(label->get_layout_rect().size.x <= 150.F + 0.5F);
		CHECK(label->get_layout_rect().size.y > test_gpu().font->measure_text("A", 14.F).y * 1.5F);
	}

	TEST_CASE("a word wider than its label is drawn on the lines the layout made for it") {
		Fixture narrow(120.F);
		auto *label = narrow.box->add_child<Label>(std::string("Tiles_GreenAndGoldPattern_01_basecolor.jpg"));
		narrow.layout();
		const Rect rect = label->get_layout_rect();
		const F32 size = label->get_display_style().font_size;
		Text::FontAtlas &font = *test_gpu().font;
		const std::string &text = label->get_text();

		const std::vector<Rendering::TextLine> lines = Rendering::wrap_text(text, font, size, rect.size.x);
		CHECK(lines.size() > 1);
		CHECK(static_cast<F32>(lines.size()) * font.get_line_height() * size == doctest::Approx(rect.size.y));
		for (const Rendering::TextLine &line : lines) {
			const std::string_view piece = std::string_view(text).substr(line.start, line.end - line.start);
			CHECK(font.measure_text(piece, size).x <= rect.size.x + 0.5F);
		}
		CHECK(lines.front().start == 0);
		CHECK(lines.back().end == text.size());
	}

	TEST_CASE("wrapped text still breaks at spaces and keeps blank lines") {
		Text::FontAtlas &font = *test_gpu().font;
		const std::string text = "one two\n\nthree";
		const std::vector<Rendering::TextLine> lines = Rendering::wrap_text(text, font, 14.F, 1000.F);
		REQUIRE(lines.size() == 3);
		CHECK(text.substr(lines[0].start, lines[0].end - lines[0].start) == "one two");
		CHECK(lines[1].start == lines[1].end);
		CHECK(text.substr(lines[2].start, lines[2].end - lines[2].start) == "three");

		const F32 narrow = std::max(font.measure_text("one", 14.F).x, font.measure_text("two", 14.F).x) + 1.F;
		const std::vector<Rendering::TextLine> broken = Rendering::wrap_text("one two", font, 14.F, narrow);
		REQUIRE(broken.size() == 2);
		CHECK(broken[1].start == 4);
	}

	TEST_CASE("a one-line label with a middle ellipsis keeps both ends of its text") {
		Fixture narrow(120.F);
		Label *label = one_line_label(*narrow.box, TextOverflow::EllipsisMiddle);
		narrow.layout();
		const Rect rect = label->get_layout_rect();
		CHECK(rect.size.x <= 120.F + 0.5F);
		const std::string text = drawn_text(*label);
		CHECK(text.starts_with("Tiles"));
		CHECK(text.ends_with(".jpg"));
		CHECK(has_ellipsis(text));
		CHECK(test_gpu().font->measure_text(text, label->get_display_style().font_size).x <= rect.size.x + 0.5F);
	}

	TEST_CASE("a one-line label with an end ellipsis keeps the start of its text") {
		Fixture narrow(120.F);
		Label *label = one_line_label(*narrow.box, TextOverflow::Ellipsis);
		narrow.layout();
		const std::string text = drawn_text(*label);
		CHECK(text.starts_with("Tiles"));
		CHECK(!text.ends_with(".jpg"));
		CHECK(has_ellipsis(text));
		CHECK(test_gpu().font->measure_text(text, label->get_display_style().font_size).x <=
			  label->get_layout_rect().size.x + 0.5F);
	}

	TEST_CASE("a one-line label that fits is drawn as it is") {
		Fixture wide(2000.F);
		Label *label = one_line_label(*wide.box, TextOverflow::EllipsisMiddle);
		wide.layout();
		CHECK(drawn_text(*label) == K_FILE_NAME);
	}

	TEST_CASE("a filled asset slot with a long name is as tall as a number field") {
		Fixture narrow(160.F);
		auto *slot = narrow.box->add_child<AssetSlot>(std::string("texture"));
		slot->add_class("split-AssetSlot");
		slot->set_value({ .asset_path = std::string("/project/Content/") + K_FILE_NAME, .asset_type = "texture" });
		auto *field = narrow.box->add_child<DragFloat>();
		field->add_class("split-DragFloat");
		narrow.layout();
		CHECK(slot->get_layout_rect().size.y == doctest::Approx(field->get_layout_rect().size.y));
		CHECK(slot->get_tooltip() == std::string("/project/Content/") + K_FILE_NAME);
	}

	TEST_CASE("an asset slot never draws its name over its clear button") {
		Aquila::UI::Core::FontRegistry::set_ui_scale(1.05F);
		Aquila::UI::Core::TextureCache textures(*test_gpu().ctx);
		GFX::GfxTexture *thumbnail = textures.load("/resources/Engine/aquila-logo.png");
		REQUIRE(thumbnail != nullptr);
		bool loaded = false;
		AssetSlot::set_adapter("texture",
							   { .thumbnail = [&](const std::string &) { return loaded ? thumbnail : nullptr; } });

		for (F32 width = 90.F; width <= 420.F; width += 11.F) {
			CAPTURE(width);
			loaded = false;
			Fixture panel(width);
			auto *slot = panel.box->add_child<AssetSlot>(std::string("texture"));
			slot->add_class("split-AssetSlot");
			slot->set_value({ .asset_path = std::string("/project/Content/") + K_FILE_NAME, .asset_type = "texture" });
			panel.layout();
			loaded = true;
			AssetSlot::refresh_thumbnails(slot->get_value().asset_path);
			panel.layout();

			const auto *label = dynamic_cast<const Label *>(slot->get_children()[1].get());
			const View &clear = *slot->get_children()[2];
			REQUIRE(label != nullptr);
			CHECK(slot->get_children()[0]->get_layout_rect().size.x > 0.F);
			CHECK(label->get_absolute_rect().right() <= clear.get_absolute_rect().left() + 0.5F);
			const std::string text = drawn_text(const_cast<Label &>(*label));
			const F32 drawn = test_gpu().font->measure_text(text, label->get_display_style().font_size).x;
			CHECK(label->get_absolute_rect().left() + drawn <= clear.get_absolute_rect().left() + 0.5F);
		}
		AssetSlot::set_adapter("texture", {});
		Aquila::UI::Core::FontRegistry::set_ui_scale(1.F);
	}

	TEST_CASE("a dock tab keeps its title on one line and stays inside the tab bar") {
		Fixture narrow(90.F, Overflow::Hidden, true);
		auto *strip = narrow.box->add_child<View>();
		strip->add_class("dock-tab-strip");
		StyleProperties strip_size;
		strip_size.width = StyleLength::percent(100.F);
		strip_size.height = StyleLength::pixel(28.F);
		strip->merge_style(strip_size);
		auto *tab = strip->add_child<Button>(std::string("Content Browser And Everything Else"));
		tab->add_class("dock-tab-btn");
		narrow.layout();

		check_one_line(*tab);
		CHECK(tab->get_absolute_rect().right() <= strip->get_absolute_rect().right() + 0.5F);
		CHECK(tab->get_display_style().overflow == Overflow::Clip);
	}

	TEST_CASE("a tab view tab keeps its title on one line") {
		Fixture narrow(90.F);
		auto *tab = narrow.box->add_child<Button>(std::string("Rendering And Post Processing"));
		tab->add_class("tab-button");
		narrow.layout();
		check_one_line(*tab);
		CHECK(tab->get_absolute_rect().right() <= narrow.box->get_absolute_rect().right() + 0.5F);
	}

	TEST_CASE("a hierarchy row keeps its name on one line and stays inside the panel") {
		Fixture narrow(110.F);
		auto *tree = narrow.box->add_child<TreeView>();
		TreeNode *node = tree->add_node("Directional Light With A Very Long Name");
		REQUIRE(node != nullptr);
		narrow.layout();

		check_one_line(*node);
		CHECK(node->get_header()->get_absolute_rect().right() <= narrow.box->get_absolute_rect().right() + 0.5F);
	}
}
