#include <doctest.h>

#include "CanvasTestSupport.h"
#include "GpuTestSupport.h"

#include "Aquila/Foundation/Reflection/TypeInfo.h"
#include "Aquila/UI/Core/Canvas.h"
#include "Aquila/UI/Core/LayoutLoader.h"
#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/Collapsible.h"
#include "Aquila/UI/Widgets/DragFloat.h"
#include "Aquila/UI/Widgets/InspectorView.h"
#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/PropertyGrid.h"
#include "Aquila/UI/Widgets/TextInput.h"

using namespace Aquila::UI::Core;
using namespace Aquila::Reflection;

namespace {

struct Light {
	float intensity = 1.F;
};

const TypeInfo &light_type() {
	static const TypeInfo type = [] {
		TypeInfo info("Light");
		TypeBuilder<Light>(info).property("Intensity", &Light::intensity);
		return info;
	}();
	return type;
}

class RecordingContent final : public IInspectorContent {
  public:
	explicit RecordingContent(std::vector<std::string> &log) : m_log(log) {}
	~RecordingContent() override { m_log.emplace_back("destroyed"); }

	void build_above(PropertyGrid &grid) override {
		m_log.emplace_back("above");
		grid.add_row<Label>("Above", std::string("above"));
	}
	void build_below(PropertyGrid &grid) override {
		m_log.emplace_back("below");
		grid.add_row<Label>("Below", std::string("below"));
	}
	void refresh() override { m_log.emplace_back("refresh"); }
	void on_property_edited() override { m_log.emplace_back("content edited"); }

  private:
	std::vector<std::string> &m_log;
};

InspectorSection light_section(Light &light, std::string id, std::string title = "Light") {
	InspectorSection section;
	section.id = std::move(id);
	section.title = std::move(title);
	section.type = &light_type();
	section.instance = [&light]() -> void * { return &light; };
	return section;
}

InspectorTab tab(std::string title, bool always_visible = false) {
	InspectorTab result;
	result.title = std::move(title);
	result.always_visible = always_visible;
	return result;
}

Inspectable with_tabs(std::vector<InspectorTab> tabs) {
	Inspectable inspectable;
	inspectable.title = "Thing";
	inspectable.tabs = std::move(tabs);
	return inspectable;
}

Inspectable one_tab(InspectorSection section) {
	std::vector<InspectorTab> tabs;
	tabs.push_back(tab("Main"));
	tabs.back().sections.push_back(std::move(section));
	return with_tabs(std::move(tabs));
}

std::string read_file(const std::string &path) {
	std::ifstream file(path);
	std::stringstream text;
	text << file.rdbuf();
	return text.str();
}

struct Fixture {
	Unique<View> root;
	Unique<InspectorView> view;

	Fixture() {
		LayoutLoader loader;
		root =
			loader.load_string(read_file(std::string(AQUILA_SOURCE_DIR) + "/Resources/Engine/UI/inspector.aqlayout"));
		REQUIRE(root != nullptr);
		view = std::make_unique<InspectorView>(*root, InspectorView::IconResolver{}, nullptr);
	}
};

View &section_view(const InspectorView &view, std::string_view id) {
	View *found = view.find_section(id);
	REQUIRE(found != nullptr);
	return *found;
}

bool is_hidden(const View *view) {
	REQUIRE(view != nullptr);
	return std::ranges::find(view->get_classes(), "hidden") != view->get_classes().end();
}

template <typename T> void collect(const View &view, std::vector<T *> &out) {
	for (const auto &child : view.get_children()) {
		if (auto *found = dynamic_cast<T *>(child.get())) {
			out.push_back(found);
		}
		collect(*child, out);
	}
}

template <typename T> std::vector<T *> all(const View &view) {
	std::vector<T *> out;
	collect(view, out);
	return out;
}

std::vector<Button *> with_class(const View &view, std::string_view cls) {
	std::vector<Button *> out;
	for (Button *button : all<Button>(view)) {
		if (std::ranges::find(button->get_classes(), cls) != button->get_classes().end()) {
			out.push_back(button);
		}
	}
	return out;
}

Label *label_with_text(const View &view, std::string_view text) {
	for (Label *label : all<Label>(view)) {
		if (label->get_text() == text) {
			return label;
		}
	}
	return nullptr;
}

Usize depth_first_index(const View &root, const View *target) {
	Usize index = 0;
	Usize found = 0;
	const auto visit = [&](const auto &self, const View &view) -> void {
		for (const auto &child : view.get_children()) {
			++index;
			if (child.get() == target) {
				found = index;
			}
			self(self, *child);
		}
	};
	visit(visit, root);
	REQUIRE(found != 0);
	return found;
}

}

TEST_SUITE("InspectorView") {
	TEST_CASE("a new view shows the empty state") {
		Fixture f;
		CHECK_FALSE(f.view->is_showing());
		CHECK_FALSE(is_hidden(f.root->find_by_id("inspector-empty")));
		CHECK(is_hidden(f.root->find_by_id("inspector-scroll")));
	}

	TEST_CASE("sections are drawn in order, titled ones as collapsibles") {
		Light first;
		Light second;
		Fixture f;
		std::vector<InspectorTab> tabs;
		tabs.push_back(tab("Main"));
		tabs.back().sections.push_back(light_section(first, "a", "A"));
		tabs.back().sections.push_back(light_section(second, "b", ""));
		f.view->show(with_tabs(std::move(tabs)));

		CHECK(f.view->is_showing());
		CHECK(is_hidden(f.root->find_by_id("inspector-empty")));
		View *a = f.view->find_section("a");
		View *b = f.view->find_section("b");
		REQUIRE(a != nullptr);
		REQUIRE(b != nullptr);
		CHECK(dynamic_cast<Collapsible *>(a) != nullptr);
		CHECK(dynamic_cast<Collapsible *>(b) == nullptr);
		CHECK(depth_first_index(*f.root, a) < depth_first_index(*f.root, b));
		CHECK(f.view->find_section("c") == nullptr);
	}

	TEST_CASE("the header shows title and subtitle, the title is editable only with rename") {
		Light light;
		std::string renamed;
		Fixture f;
		Inspectable inspectable = one_tab(light_section(light, "light"));
		inspectable.title = "Cube";
		inspectable.subtitle = "1a2b3c4d";
		inspectable.subtitle_tooltip = "1a2b3c4d-full";
		inspectable.rename = [&renamed](const std::string &name) { renamed = name; };
		f.view->show(std::move(inspectable));

		auto *name = f.root->find_by_id<TextInput>("inspector-name");
		auto *uuid = f.root->find_by_id<Label>("inspector-uuid");
		REQUIRE(name != nullptr);
		REQUIRE(uuid != nullptr);
		CHECK(name->get_text() == "Cube");
		CHECK(name->is_enabled());
		CHECK(uuid->get_text() == "1a2b3c4d");
		CHECK(uuid->get_tooltip() == "1a2b3c4d-full");
		name->on_submit("Box");
		CHECK(renamed == "Box");

		f.view->show(one_tab(light_section(light, "light")));
		CHECK_FALSE(name->is_enabled());
	}

	TEST_CASE("one tab draws no tab strip, empty tabs hide unless always visible") {
		Light light;
		Fixture f;
		f.view->show(one_tab(light_section(light, "light")));
		CHECK(is_hidden(f.root->find_by_id("props-tabs")));

		std::vector<InspectorTab> tabs;
		tabs.push_back(tab("Object"));
		tabs.back().sections.push_back(light_section(light, "light"));
		tabs.push_back(tab("Components", true));
		tabs.push_back(tab("Rendering"));
		f.view->show(with_tabs(std::move(tabs)));

		CHECK_FALSE(is_hidden(f.root->find_by_id("props-tabs")));
		const std::vector<Button *> buttons = with_class(*f.root->find_by_id("props-tabs"), "props-tab");
		REQUIRE(buttons.size() == 3);
		CHECK_FALSE(is_hidden(buttons[0]));
		CHECK_FALSE(is_hidden(buttons[1]));
		CHECK(is_hidden(buttons[2]));
	}

	TEST_CASE("the active tab carries over to the next inspectable when it has it") {
		Light light;
		Fixture f;
		const auto two_tabs = [&light](std::string second) {
			std::vector<InspectorTab> tabs;
			tabs.push_back(tab("A"));
			tabs.back().sections.push_back(light_section(light, "a"));
			tabs.push_back(tab(std::move(second)));
			tabs.back().sections.push_back(light_section(light, "b"));
			return with_tabs(std::move(tabs));
		};
		f.view->show(two_tabs("B"));
		f.view->select_tab("B");
		f.view->show(two_tabs("B"));
		CHECK(f.view->get_active_tab() == "B");

		f.view->show(two_tabs("C"));
		CHECK(f.view->get_active_tab() == "A");
	}

	TEST_CASE("an edit reaches the instance, then the content, then the section") {
		Light light;
		std::vector<std::string> log;
		Fixture f;
		InspectorSection section = light_section(light, "light");
		section.content = std::make_unique<RecordingContent>(log);
		section.on_edited = [&log] { log.emplace_back("section edited"); };
		f.view->show(one_tab(std::move(section)));

		const std::vector<DragFloat *> fields = all<DragFloat>(section_view(*f.view, "light"));
		REQUIRE(fields.size() == 1);
		fields[0]->on_changed(5.F);
		CHECK(light.intensity == doctest::Approx(5.F));
		CHECK(log == std::vector<std::string>{ "above", "below", "content edited", "section edited" });
	}

	TEST_CASE("content goes above and below the reflected properties") {
		Light light;
		std::vector<std::string> log;
		Fixture f;
		InspectorSection section = light_section(light, "light");
		section.content = std::make_unique<RecordingContent>(log);
		f.view->show(one_tab(std::move(section)));

		const View *root = &section_view(*f.view, "light");
		const std::vector<DragFloat *> fields = all<DragFloat>(*root);
		REQUIRE(fields.size() == 1);
		const Label *above = label_with_text(*root, "above");
		const Label *below = label_with_text(*root, "below");
		REQUIRE(above != nullptr);
		REQUIRE(below != nullptr);
		CHECK(depth_first_index(*root, above) < depth_first_index(*root, fields[0]));
		CHECK(depth_first_index(*root, fields[0]) < depth_first_index(*root, below));
	}

	TEST_CASE("refresh asks for the instance again and reads it") {
		Light first{ .intensity = 1.F };
		Light second{ .intensity = 7.F };
		Light *current = &first;
		std::vector<std::string> log;
		Fixture f;
		InspectorSection section = light_section(first, "light");
		section.instance = [&current]() -> void * { return current; };
		section.content = std::make_unique<RecordingContent>(log);
		f.view->show(one_tab(std::move(section)));

		current = &second;
		f.view->refresh();
		const std::vector<DragFloat *> fields = all<DragFloat>(section_view(*f.view, "light"));
		REQUIRE(fields.size() == 1);
		CHECK(fields[0]->get_value() == doctest::Approx(7.F));
		CHECK(std::ranges::find(log, "refresh") != log.end());
	}

	TEST_CASE("a section whose instance is gone is hidden") {
		Light light;
		bool alive = true;
		Fixture f;
		InspectorSection section = light_section(light, "light");
		section.instance = [&light, &alive]() -> void * { return alive ? &light : nullptr; };
		f.view->show(one_tab(std::move(section)));
		CHECK_FALSE(is_hidden(f.view->find_section("light")));

		alive = false;
		f.view->refresh();
		CHECK(is_hidden(f.view->find_section("light")));
	}

	TEST_CASE("actions run, disabled ones can't be clicked") {
		Light light;
		int runs = 0;
		Fixture f;
		Inspectable inspectable = one_tab(light_section(light, "light"));
		inspectable.tabs[0].actions.push_back({ .text = "Add", .run = [&runs] { ++runs; } });
		inspectable.tabs[0].actions.push_back({ .text = "Never", .run = [] {}, .enabled = [] { return false; } });
		f.view->show(std::move(inspectable));

		const std::vector<Button *> actions = with_class(*f.root, "props-action");
		REQUIRE(actions.size() == 2);
		actions[0]->on_click();
		CHECK(runs == 1);
		CHECK(actions[0]->is_enabled());
		CHECK_FALSE(actions[1]->is_enabled());
	}

	TEST_CASE("showing again from inside an action keeps the old one alive until update") {
		Light light;
		std::vector<std::string> log;
		Fixture f;
		InspectorSection section = light_section(light, "light");
		section.content = std::make_unique<RecordingContent>(log);
		Inspectable inspectable = one_tab(std::move(section));
		inspectable.tabs[0].actions.push_back(
			{ .text = "Again", .run = [&f, &light] { f.view->show(one_tab(light_section(light, "light"))); } });
		f.view->show(std::move(inspectable));

		const std::vector<Button *> actions = with_class(*f.root, "props-action");
		REQUIRE(actions.size() == 1);
		actions[0]->on_click();
		CHECK(std::ranges::find(log, "destroyed") == log.end());
		CHECK(f.view->find_section("light") != nullptr);

		f.view->update();
		CHECK(std::ranges::find(log, "destroyed") != log.end());
	}

	TEST_CASE("a collapsed section stays collapsed for the next inspectable with its id") {
		Light light;
		Fixture f;
		f.view->show(one_tab(light_section(light, "light")));
		auto *section = dynamic_cast<Collapsible *>(f.view->find_section("light"));
		REQUIRE(section != nullptr);
		CHECK(section->is_expanded());
		section->set_expanded(false);
		section->on_toggled(false);

		f.view->show(one_tab(light_section(light, "light")));
		auto *again = dynamic_cast<Collapsible *>(f.view->find_section("light"));
		REQUIRE(again != nullptr);
		CHECK_FALSE(again->is_expanded());

		f.view->show(one_tab(light_section(light, "other")));
		auto *other = dynamic_cast<Collapsible *>(f.view->find_section("other"));
		REQUIRE(other != nullptr);
		CHECK(other->is_expanded());
	}

	TEST_CASE("the canvas can go away while something is shown") {
		struct Owner final : View {
			Unique<InspectorView> inspector;
			~Owner() override { inspector.reset(); }
		};
		Light light;
		test_gpu();
		CanvasSingletonsScope singletons;
		auto canvas = std::make_unique<Canvas>(320, 480);
		auto *owner = canvas->get_root()->add_child<Owner>();
		Aquila::UI::StyleProperties style;
		style.font_family = "regular";
		owner->merge_style(style);
		LayoutLoader loader;
		View *layout = owner->add_child(
			loader.load_string(read_file(std::string(AQUILA_SOURCE_DIR) + "/Resources/Engine/UI/inspector.aqlayout")));
		REQUIRE(layout != nullptr);
		owner->inspector = std::make_unique<InspectorView>(*layout, InspectorView::IconResolver{}, nullptr);
		owner->inspector->show(one_tab(light_section(light, "light")));
		canvas->reload_styles();
		canvas->update(0.016F);
		canvas->compute();

		canvas.reset();
		CHECK(canvas == nullptr);
	}

	TEST_CASE("clear goes back to the empty state") {
		Light light;
		Fixture f;
		f.view->show(one_tab(light_section(light, "light")));
		f.view->clear();
		CHECK_FALSE(f.view->is_showing());
		CHECK(f.view->find_section("light") == nullptr);
		CHECK_FALSE(is_hidden(f.root->find_by_id("inspector-empty")));
		CHECK(is_hidden(f.root->find_by_id("inspector-scroll")));
	}
}
