#include <doctest.h>

#include "Aquila/UI/Core/Behaviour.h"
#include "Aquila/UI/Core/Command.h"
#include "Aquila/UI/Core/LayoutLoader.h"
#include "Aquila/UI/Core/Shortcuts.h"
#include "Aquila/UI/Widgets/AssetCard.h"
#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/Checkbox.h"
#include "Aquila/UI/Widgets/ColorPicker.h"
#include "Aquila/UI/Widgets/Dropdown.h"
#include "Aquila/UI/Widgets/Image.h"
#include "Aquila/UI/Widgets/IconLabel.h"
#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/ListBox.h"
#include "Aquila/UI/Widgets/PopupMenu.h"
#include "Aquila/UI/Widgets/ProgressBar.h"
#include "Aquila/UI/Widgets/Separator.h"
#include "Aquila/UI/Widgets/Slider.h"
#include "Aquila/UI/Widgets/TabBar.h"
#include "Aquila/UI/Widgets/TabView.h"
#include "Aquila/UI/Widgets/TextInput.h"
#include "Aquila/UI/Widgets/Toggle.h"
#include "Aquila/UI/Widgets/TreeView.h"
#include "GpuTestSupport.h"
#include "VfsTestSupport.h"

using namespace Aquila::UI::Core;
using Aquila::GFX::GfxTexture;

namespace {

class IconLoader : public LayoutLoader {
  public:
	[[nodiscard]] GfxTexture *resolve_texture(const std::string &path) const override {
		auto [it, inserted] = m_textures.try_emplace(path, nullptr);
		if (inserted) {
			it->second = std::make_unique<char>();
		}
		return reinterpret_cast<GfxTexture *>(it->second.get());
	}

  private:
	mutable std::map<std::string, Unique<char>> m_textures;
};

bool has_class(const View &view, std::string_view cls) {
	return std::ranges::find(view.get_classes(), cls) != view.get_classes().end();
}

template <typename T> T *root_as(const Unique<View> &root) {
	REQUIRE(root != nullptr);
	auto *typed = dynamic_cast<T *>(root.get());
	REQUIRE(typed != nullptr);
	return typed;
}

std::string text_of(const Button &button) {
	REQUIRE(button.get_content() != nullptr);
	return button.get_content()->get_label()->get_text();
}

std::string shortcut_of(const Button &button) {
	REQUIRE(button.get_content() != nullptr);
	const Label *shortcut = button.get_content()->get_shortcut();
	return shortcut != nullptr ? shortcut->get_text() : std::string();
}

GfxTexture *icon_of(const Button &button) {
	REQUIRE(button.get_content() != nullptr);
	return button.get_content()->get_icon()->get_texture();
}

const Button &dropdown_header(const Dropdown &dropdown) {
	const auto *header = dynamic_cast<const Button *>(dropdown.get_children().front().get());
	REQUIRE(header != nullptr);
	return *header;
}

void collect_buttons(const View &view, std::vector<std::string> &out) {
	for (const auto &child : view.get_children()) {
		if (const auto *button = dynamic_cast<const Button *>(child.get())) {
			out.push_back(text_of(*button));
			continue;
		}
		collect_buttons(*child, out);
	}
}

void collect_nodes(const View &view, std::vector<std::pair<std::string, int>> &out) {
	for (const auto &child : view.get_children()) {
		if (const auto *node = dynamic_cast<const TreeNode *>(child.get())) {
			out.emplace_back(node->get_label(), node->get_depth());
		}
		collect_nodes(*child, out);
	}
}

std::vector<const View *> menu_rows(PopupMenu &menu) {
	menu.refresh();
	std::vector<const View *> rows;
	for (const auto &child : menu.get_children()) {
		if (has_class(*child, "menu-item") || has_class(*child, "menu-separator")) {
			rows.push_back(child.get());
		}
	}
	return rows;
}

std::vector<std::string> menu_texts(PopupMenu &menu) {
	std::vector<std::string> texts;
	for (const View *row : menu_rows(menu)) {
		const auto *button = dynamic_cast<const Button *>(row);
		texts.push_back(button != nullptr ? text_of(*button) : std::string("---"));
	}
	return texts;
}

const Button &menu_button(PopupMenu &menu, Usize index) {
	const std::vector<const View *> rows = menu_rows(menu);
	REQUIRE(index < rows.size());
	const auto *button = dynamic_cast<const Button *>(rows[index]);
	REQUIRE(button != nullptr);
	return *button;
}

PopupMenu *first_submenu(PopupMenu &menu) {
	for (const auto &child : menu.get_children()) {
		if (auto *submenu = dynamic_cast<PopupMenu *>(child.get())) {
			return submenu;
		}
	}
	return nullptr;
}

}

TEST_SUITE("Markup items") {
	TEST_CASE("a dropdown takes its options from markup") {
		IconLoader loader;
		const Unique<View> root = loader.load_string(R"(<Dropdown>
	<Option value="cube" label="Cube" icon="cube.svg"/>
	<Option value="sphere"/>
</Dropdown>)");
		auto *dropdown = root_as<Dropdown>(root);

		dropdown->set_value("cube");
		CHECK(dropdown->get_value() == "cube");
		CHECK(text_of(dropdown_header(*dropdown)) == "Cube");
		CHECK(icon_of(dropdown_header(*dropdown)) == loader.resolve_texture("cube.svg"));

		dropdown->set_value("sphere");
		CHECK(dropdown->get_value() == "sphere");
		CHECK(text_of(dropdown_header(*dropdown)) == "sphere");
	}

	TEST_CASE("item labels show as written") {
		LogCapture log;
		LayoutLoader loader;
		const Unique<View> plain = loader.load_string(R"(<Dropdown><Option value="a" label="$1 & <b>"/></Dropdown>)");
		auto *dropdown = root_as<Dropdown>(plain);
		dropdown->set_value("a");
		CHECK(text_of(dropdown_header(*dropdown)) == "$1 & <b>");

		const Unique<View> built = loader.load_string(R"(<Component name="Picker">
	<Dropdown><Option value="a" label="$1 & <b>"/></Dropdown>
</Component>
<Picker/>)");
		auto *in_component = root_as<Dropdown>(built);
		in_component->set_value("a");
		CHECK(text_of(dropdown_header(*in_component)) == "$1 & <b>");
		CHECK_MESSAGE(log.stream.str().empty(), log.stream.str());
	}

	TEST_CASE("a list box takes its items from markup") {
		LayoutLoader loader;
		const Unique<View> root = loader.load_string(R"(<ListBox>
	<Item value="a" label="Alpha"/>
	<Item value="b" label="Bravo"/>
</ListBox>)");
		auto *list = root_as<ListBox>(root);

		std::vector<std::string> labels;
		collect_buttons(*list, labels);
		CHECK(labels == std::vector<std::string>{ "Alpha", "Bravo" });
		list->set_selected_id("b");
		CHECK(list->get_selected_id() == "b");
	}

	TEST_CASE("a tab view takes its tabs from markup, their children become the page") {
		LayoutLoader loader;
		const Unique<View> root = loader.load_string(R"(<TabView>
	<Tab title="One"><Label id="first">First</Label></Tab>
	<Tab title="Two"><Label id="second">Second</Label><Label>More</Label></Tab>
</TabView>)");
		auto *tabs = root_as<TabView>(root);

		const auto *bar = dynamic_cast<const TabBar *>(tabs->get_children()[0].get());
		REQUIRE(bar != nullptr);
		REQUIRE(bar->get_tab_count() == 2);
		CHECK(text_of(*bar->get_tab(0)) == "One");
		CHECK(text_of(*bar->get_tab(1)) == "Two");
		CHECK(tabs->get_active_tab() == 0);

		const View *first = tabs->find_by_id("first");
		const View *second = tabs->find_by_id("second");
		REQUIRE(first != nullptr);
		REQUIRE(second != nullptr);
		CHECK(first->get_parent() != second->get_parent());
		CHECK(second->get_parent()->get_children().size() == 2);
		CHECK(has_class(*second->get_parent(), "hidden"));
		CHECK_FALSE(has_class(*first->get_parent(), "hidden"));
	}

	TEST_CASE("a tab bar takes its tabs from markup") {
		IconLoader loader;
		const Unique<View> root = loader.load_string(R"(<TabBar tab-class="nav">
	<Tab title="Recent" icon="clock.svg" tooltip="Your projects"/>
	<Tab title="New"/>
</TabBar>)");
		auto *bar = root_as<TabBar>(root);

		REQUIRE(bar->get_tab_count() == 2);
		const Button &recent = *bar->get_tab(0);
		CHECK(text_of(recent) == "Recent");
		CHECK(recent.get_tooltip() == "Your projects");
		CHECK(icon_of(recent) == loader.resolve_texture("clock.svg"));
		CHECK(has_class(recent, "nav"));
		CHECK(text_of(*bar->get_tab(1)) == "New");
	}

	TEST_CASE("a tree view takes nested nodes from markup") {
		LayoutLoader loader;
		const Unique<View> root = loader.load_string(R"(<TreeView>
	<TreeNode label="Root">
		<TreeNode label="A"/>
		<TreeNode label="B"><TreeNode label="Deep"/></TreeNode>
	</TreeNode>
	<TreeNode label="Second"/>
</TreeView>)");
		auto *tree = root_as<TreeView>(root);

		using Nodes = std::vector<std::pair<std::string, int>>;
		Nodes nodes;
		collect_nodes(*tree, nodes);
		CHECK(nodes == Nodes{ { "Root", 0 }, { "A", 1 }, { "B", 1 }, { "Deep", 2 }, { "Second", 0 } });
	}

	TEST_CASE("a popup menu takes items, separators and submenus from markup") {
		IconLoader loader;
		const Unique<View> root = loader.load_string(R"(<PopupMenu>
	<MenuItem label="Plain" icon="dot.svg"/>
	<MenuSeparator/>
	<Submenu label="More" icon="more.svg">
		<MenuItem label="Inner"/>
		<Submenu label="Deeper"><MenuItem label="Deepest"/></Submenu>
	</Submenu>
</PopupMenu>)");
		auto *menu = root_as<PopupMenu>(root);

		CHECK(menu_texts(*menu) == std::vector<std::string>{ "Plain", "---", "More" });
		CHECK(icon_of(menu_button(*menu, 0)) == loader.resolve_texture("dot.svg"));
		CHECK(icon_of(menu_button(*menu, 2)) == loader.resolve_texture("more.svg"));

		PopupMenu *more = first_submenu(*menu);
		REQUIRE(more != nullptr);
		CHECK(menu_texts(*more) == std::vector<std::string>{ "Inner", "Deeper" });
		PopupMenu *deeper = first_submenu(*more);
		REQUIRE(deeper != nullptr);
		CHECK(menu_texts(*deeper) == std::vector<std::string>{ "Deepest" });
	}

	TEST_CASE("a menu item with a command shows its title and shortcut and runs it") {
		Commands commands;
		Shortcuts shortcuts(commands);
		int saved = 0;
		commands.add({ .id = "scene.save", .title = "Save Scene", .run = [&saved] { ++saved; } });
		commands.add({ .id = "scene.save_as", .title = "Save Scene As" });
		shortcuts.bind("scene.save", "Ctrl+S");
		LayoutLoader loader;
		loader.set_command_source(&commands, &shortcuts);

		const Unique<View> root = loader.load_string(R"(<PopupMenu>
	<MenuItem command="scene.save"/>
	<MenuItem command="scene.save_as" label="Save As..."/>
</PopupMenu>)");
		auto *menu = root_as<PopupMenu>(root);

		CHECK(menu_texts(*menu) == std::vector<std::string>{ "Save Scene", "Save As..." });
		REQUIRE_FALSE(shortcuts.get_label("scene.save").empty());
		CHECK(shortcut_of(menu_button(*menu, 0)) == shortcuts.get_label("scene.save"));
		menu_button(*menu, 0).on_click();
		CHECK(saved == 1);
	}

	TEST_CASE("a menu item with an unknown command says where it is written") {
		LogCapture log;
		Commands commands;
		LayoutLoader loader;
		loader.set_command_source(&commands);
		const Unique<View> root = loader.load_string("<PopupMenu>\n\t<MenuItem command=\"nope\"/>\n</PopupMenu>");
		auto *menu = root_as<PopupMenu>(root);
		CHECK(menu_texts(*menu) == std::vector<std::string>{ "nope" });
		CHECK(log.stream.str().find("<string>:2:") != std::string::npos);
		CHECK(log.stream.str().find("nope") != std::string::npos);
	}

	TEST_CASE("a menu item with an action gets what it runs from code") {
		LayoutLoader loader;
		const Unique<View> root = loader.load_string(R"(<PopupMenu>
	<MenuItem action="open" label="Open"/>
	<Submenu label="More"><MenuItem action="delete" label="Delete"/></Submenu>
</PopupMenu>)");
		auto *menu = root_as<PopupMenu>(root);
		int opened = 0;
		int deleted = 0;
		CHECK(menu->set_action("open", [&opened] { ++opened; }));
		CHECK(menu->set_action("delete", [&deleted] { ++deleted; }));
		CHECK_FALSE(menu->set_action("missing", [] {}));

		menu_button(*menu, 0).on_click();
		CHECK(opened == 1);
		PopupMenu *more = first_submenu(*menu);
		REQUIRE(more != nullptr);
		menu_button(*more, 0).on_click();
		CHECK(deleted == 1);
	}

	TEST_CASE("a menu item's command isn't checked when the loader has no commands") {
		LogCapture log;
		LayoutLoader loader;
		const Unique<View> root = loader.load_string(R"(<PopupMenu><MenuItem command="scene.save"/></PopupMenu>)");
		auto *menu = root_as<PopupMenu>(root);
		CHECK(menu_texts(*menu) == std::vector<std::string>{ "scene.save" });
		CHECK_MESSAGE(log.stream.str().empty(), log.stream.str());
	}

	TEST_CASE("a submenu's id is found by the behaviour of the markup that wrote it") {
		struct Finder : Behaviour {
			PopupMenu *found = nullptr;
			void define_behaviour() override { found = get<PopupMenu>("more"); }
		};
		LayoutLoader loader;
		const Aquila::Foundation::Registration registration = loader.behaviours().add<Finder>("Finder");
		const Unique<View> root = loader.load_string(R"(<Component name="Menu" behaviour="Finder">
	<PopupMenu><Submenu id="more" label="More"><MenuItem label="Inner"/></Submenu></PopupMenu>
</Component>
<Menu/>)");
		REQUIRE(root != nullptr);
		auto *finder = find_behaviour<Finder>(*root);
		REQUIRE(finder != nullptr);
		REQUIRE(finder->found != nullptr);
		CHECK(menu_texts(*finder->found) == std::vector<std::string>{ "Inner" });
	}

	TEST_CASE("items from code go between the markup items") {
		LayoutLoader loader;
		const Unique<View> root = loader.load_string(R"(<PopupMenu>
	<MenuItem label="First"/>
	<MenuItem label="Last"/>
</PopupMenu>)");
		auto *menu = root_as<PopupMenu>(root);
		int ran = 0;
		menu->insert_item(1, "Middle", "Ctrl+M", nullptr, [&ran] { ++ran; });
		menu->insert_item(99, "End", "", nullptr, {});

		CHECK(menu_texts(*menu) == std::vector<std::string>{ "First", "Middle", "Last", "End" });
		CHECK(shortcut_of(menu_button(*menu, 1)) == "Ctrl+M");
		menu_button(*menu, 1).on_click();
		CHECK(ran == 1);
	}

	TEST_CASE("a slider takes its range and value in any order") {
		LayoutLoader loader;
		const Unique<View> root = loader.load_string(R"(<Slider value="40" min="10" max="100"/>)");
		auto *slider = root_as<Slider>(root);
		CHECK(slider->get_value() == doctest::Approx(40.F));
		slider->set_value(150.F);
		CHECK(slider->get_value() == doctest::Approx(100.F));
		slider->set_value(0.F);
		CHECK(slider->get_value() == doctest::Approx(10.F));
	}

	TEST_CASE("checkboxes and toggles take checked") {
		LayoutLoader loader;
		const Unique<View> root = loader.load_string(R"(<View>
	<Checkbox id="on" checked="true"/>
	<Checkbox id="off" checked="false"/>
	<Toggle id="toggle" checked="true"/>
</View>)");
		REQUIRE(root != nullptr);
		CHECK(root->find_by_id<Checkbox>("on")->get_value());
		CHECK_FALSE(root->find_by_id<Checkbox>("off")->get_value());
		CHECK(root->find_by_id<Toggle>("toggle")->get_value());
	}

	TEST_CASE("a text input takes a placeholder") {
		LayoutLoader loader;
		const Unique<View> root = loader.load_string(R"(<TextInput placeholder="Search..."/>)");
		CHECK(root_as<TextInput>(root)->get_placeholder() == "Search...");
	}

	TEST_CASE("an asset card takes its asset") {
		LayoutLoader loader;
		const Unique<View> root =
			loader.load_string(R"(<AssetCard path="textures/wood.png" type="texture" name="wood.png"/>)");
		const AssetPayload &asset = root_as<AssetCard>(root)->get_asset();
		CHECK(asset.asset_path == "textures/wood.png");
		CHECK(asset.asset_type == "texture");
		CHECK(asset.display_name == "wood.png");
	}

	TEST_CASE("an asset card takes its thumbnail") {
		IconLoader loader;
		const Unique<View> root = loader.load_string(R"(<AssetCard path="a.png" src="info.svg"/>)");
		auto *card = root_as<AssetCard>(root);
		const auto *thumbnail = dynamic_cast<const Image *>(card->get_children().front().get());
		REQUIRE(thumbnail != nullptr);
		CHECK(thumbnail->get_texture() == loader.resolve_texture("info.svg"));
		CHECK_FALSE(has_class(*thumbnail, "hidden"));
	}

	TEST_CASE("a color picker takes its color") {
		LogCapture log;
		LayoutLoader loader;
		loader.register_widget("ColorPicker", [](std::string_view, Aquila::UI::Text::FontAtlas *) -> Unique<View> {
			return std::make_unique<ColorPicker>(*test_gpu().ctx);
		});
		const Unique<View> root = loader.load_string(R"(<View>
	<ColorPicker id="orange" color="#cc6633"/>
	<ColorPicker id="broken" color="orange-ish"/>
</View>)");
		REQUIRE(root != nullptr);
		const Vec4 orange = root->find_by_id<ColorPicker>("orange")->get_color();
		CHECK(orange.r == doctest::Approx(0.8F));
		CHECK(orange.g == doctest::Approx(0.4F));
		CHECK(orange.b == doctest::Approx(0.2F));
		CHECK(orange.a == doctest::Approx(1.F));
		CHECK(root->find_by_id<ColorPicker>("broken")->get_color() == Vec4(1.F));
		CHECK(log.stream.str().find("'orange-ish'") != std::string::npos);
	}

	TEST_CASE("bad values are reported, never thrown") {
		LogCapture log;
		LayoutLoader loader;
		Unique<View> root;
		CHECK_NOTHROW(root = loader.load_string(R"(<View>
	<Slider id="slider" min="low" max="100" value="1e999"/>
	<Checkbox id="check" checked="maybe"/>
</View>)"));
		REQUIRE(root != nullptr);
		CHECK(root->find_by_id<Slider>("slider") != nullptr);
		CHECK(log.stream.str().find("'low'") != std::string::npos);
		CHECK(log.stream.str().find("'1e999'") != std::string::npos);
		CHECK(log.stream.str().find("'maybe'") != std::string::npos);
	}

	TEST_CASE("a progress bar's bad value is reported, never thrown") {
		LogCapture log;
		LayoutLoader loader;
		Unique<View> root;
		CHECK_NOTHROW(root = loader.load_string(R"(<View>
	<ProgressBar id="bad" value="half"/>
	<ProgressBar id="good" value="0.25"/>
</View>)"));
		REQUIRE(root != nullptr);
		auto *bad = root->find_by_id<ProgressBar>("bad");
		auto *good = root->find_by_id<ProgressBar>("good");
		REQUIRE(bad != nullptr);
		REQUIRE(good != nullptr);
		CHECK(bad->get_value() == doctest::Approx(0.F));
		CHECK(good->get_value() == doctest::Approx(0.25F));
		CHECK(log.stream.str().find("'half'") != std::string::npos);
	}

	TEST_CASE("an item outside a widget that takes it says where it is and builds nothing") {
		LogCapture log;
		LayoutLoader loader;
		const Unique<View> root = loader.load_string("<View>\n\t<Label>Hi</Label>\n\t<Option value=\"a\"/>\n</View>");
		REQUIRE(root != nullptr);
		CHECK(root->get_children().size() == 1);
		CHECK(log.stream.str().find("<string>:3:") != std::string::npos);
		CHECK(log.stream.str().find("<Option>") != std::string::npos);
	}

	TEST_CASE("an item as the root or inside the wrong item builds nothing") {
		LogCapture log;
		LayoutLoader loader;
		CHECK(loader.load_string(R"(<MenuItem label="x"/>)") == nullptr);
		CHECK(log.stream.str().find("<string>:1:") != std::string::npos);

		const Unique<View> root = loader.load_string(
			"<PopupMenu>\n\t<Submenu label=\"More\">\n\t\t<Option value=\"a\"/>\n\t</Submenu>\n</PopupMenu>");
		auto *menu = root_as<PopupMenu>(root);
		PopupMenu *more = first_submenu(*menu);
		REQUIRE(more != nullptr);
		CHECK(menu_texts(*more).empty());
		CHECK(log.stream.str().find("<string>:3:") != std::string::npos);
	}

	TEST_CASE("item attributes and contents are checked") {
		LogCapture log;
		LayoutLoader loader;
		const Unique<View> root = loader.load_string(
			"<View>\n<Dropdown>\n\t<Option value=\"a\" lable=\"A\"/>\n\t<Option value=\"b\">B</Option>\n</Dropdown>\n"
			"<TabBar><Tab title=\"x\"><Label>page</Label></Tab></TabBar>\n</View>");
		REQUIRE(root != nullptr);
		const std::string text = log.stream.str();
		CHECK(text.find("<string>:3:") != std::string::npos);
		CHECK(text.find("'lable'") != std::string::npos);
		CHECK(text.find("<string>:4:") != std::string::npos);
		CHECK(text.find("<string>:6:") != std::string::npos);
	}

	TEST_CASE("a component can't take the name of an item") {
		LogCapture log;
		LayoutLoader loader;
		(void)loader.load_string(R"(<Component name="Tab"><View/></Component>
<View/>)");
		CHECK(log.stream.str().find("'Tab'") != std::string::npos);
	}
}
