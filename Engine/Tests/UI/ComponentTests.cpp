#include <doctest.h>

#include "Aquila/Foundation/Log.h"
#include "Aquila/Foundation/Registration.h"
#include "Aquila/UI/Core/Behaviour.h"
#include "Aquila/UI/Core/BehaviourRegistry.h"
#include "Aquila/UI/Core/Canvas.h"
#include "Aquila/UI/Core/ComponentArgs.h"
#include "Aquila/UI/Core/ComponentInstance.h"
#include "Aquila/UI/Core/LayoutLoader.h"
#include "Aquila/UI/Core/UIResources.h"
#include "Aquila/UI/Core/View.h"
#include "Aquila/UI/Widgets/Label.h"
#include "CanvasTestSupport.h"
#include "VfsTestSupport.h"

using namespace Aquila::UI::Core;
using Aquila::Foundation::Logger;
using Aquila::Foundation::LogLevel;

namespace {

struct Loaded {
	Unique<View> root;
	std::string log;
};

Loaded load(std::string_view xml) {
	std::ostringstream stream;
	const LogLevel previous = Logger::get_log_level();
	Logger::set_sink(&stream);
	Logger::set_log_level(LogLevel::Warning);
	LayoutLoader loader;
	Unique<View> root = loader.load_string(xml);
	Logger::set_sink(nullptr);
	Logger::set_log_level(previous);
	return { std::move(root), stream.str() };
}

std::vector<std::string> child_ids(const View &view) {
	std::vector<std::string> ids;
	for (const auto &child : view.get_children()) {
		ids.push_back(child->get_id());
	}
	return ids;
}

}

TEST_SUITE("Components") {
	TEST_CASE("a component defined in the file is used as a tag") {
		const Loaded loaded = load(R"(
<Component name="Card"><View class="card"><Label id="title">Hi</Label></View></Component>
<View id="root"><Card id="first"/><Card id="second" class="wide"/></View>)");
		REQUIRE(loaded.root != nullptr);
		CHECK_MESSAGE(loaded.log.empty(), loaded.log);
		REQUIRE(child_ids(*loaded.root) == std::vector<std::string>{ "first", "second" });
		CHECK(loaded.root->get_children()[0]->get_classes() == std::vector<std::string>{ "card" });
		CHECK(loaded.root->get_children()[1]->get_classes() == std::vector<std::string>{ "card", "wide" });
		const auto *label = dynamic_cast<const Label *>(loaded.root->get_children()[1]->get_children()[0].get());
		REQUIRE(label != nullptr);
		CHECK(label->get_text() == "Hi");
	}

	TEST_CASE("instance children go into the slot") {
		const Loaded loaded = load(R"(
<Component name="Box"><View class="box"><Label>Head</Label><Slot/><Label>Foot</Label></View></Component>
<Box id="b"><View id="inner"/><View id="second"/></Box>)");
		REQUIRE(loaded.root != nullptr);
		CHECK_MESSAGE(loaded.log.empty(), loaded.log);
		CHECK(loaded.root->get_id() == "b");
		CHECK(child_ids(*loaded.root) == std::vector<std::string>{ "", "inner", "second", "" });
	}

	TEST_CASE("a component can be built from another component") {
		const Loaded loaded = load(R"(
<Component name="A"><View class="a"/></Component>
<Component name="B"><A class="b"/></Component>
<B id="x" class="c"/>)");
		REQUIRE(loaded.root != nullptr);
		CHECK_MESSAGE(loaded.log.empty(), loaded.log);
		CHECK(loaded.root->get_id() == "x");
		CHECK(loaded.root->get_classes() == std::vector<std::string>{ "a", "b", "c" });
	}

	TEST_CASE("a component that uses itself is reported and builds nothing") {
		const Loaded loaded = load(R"(
<Component name="Loop"><View><Loop/></View></Component>
<Component name="Ping"><Pong/></Component>
<Component name="Pong"><View><Ping/></View></Component>
<View id="root"><Loop/><Ping/></View>)");
		REQUIRE(loaded.root != nullptr);
		CHECK(loaded.log.find("'Loop' uses itself") != std::string::npos);
		CHECK(loaded.log.find("'Ping' uses itself") != std::string::npos);
	}

	TEST_CASE("broken definitions are reported") {
		CHECK(load(R"(<Component><View/></Component><View/>)").log.find("needs a 'name'") != std::string::npos);
		CHECK(load(R"(<Component name="Two"><View/><View/></Component><View/>)").log.find("exactly one root") !=
			  std::string::npos);
		CHECK(load(R"(<Component name="S"><View><Slot/><Slot/></View></Component><View/>)").log.find("one <Slot/>") !=
			  std::string::npos);
		CHECK(load(R"(<Component name="D"><View/></Component><Component name="D"><View/></Component><View/>)")
				  .log.find("a second <Component> named 'D'") != std::string::npos);
		CHECK(load(R"(<Component name="Button"><View/></Component><View/>)").log.find("the name of a widget") !=
			  std::string::npos);
		CHECK(load(R"(<View><Slot/></View>)").log.find("only works inside a <Component>") != std::string::npos);
		CHECK(load(R"(<Component name="N"><View/></Component><N><View/></N>)").log.find("has no <Slot/>") !=
			  std::string::npos);
		CHECK(load(R"(<Component name="Q"><View/></Component><Q title="x"/>)")
				  .log.find("doesn't take the attribute 'title'") != std::string::npos);
	}

	TEST_CASE("instances that repeat an id each lay out in their own place") {
		CanvasSingletonsScope singletons;
		Canvas canvas(400, 200);
		Loaded loaded = load(R"(
<Component name="Box"><View style="width: 100px; height: 50px"><View id="dot" style="width: 10px; height: 10px"/></View></Component>
<View style="flex-direction: row"><Box/><Box/></View>)");
		REQUIRE(loaded.root != nullptr);
		View *layout = canvas.get_root()->add_child(std::move(loaded.root));
		for (int i = 0; i < 2; ++i) {
			canvas.update(0.016F);
			canvas.compute();
		}

		REQUIRE(layout->get_children().size() == 2);
		const View &first = *layout->get_children()[0]->get_children()[0];
		const View &second = *layout->get_children()[1]->get_children()[0];
		CHECK(first.get_absolute_position().x == doctest::Approx(0.F));
		CHECK(second.get_absolute_position().x == doctest::Approx(100.F));
	}
}

TEST_SUITE("Component props") {
	TEST_CASE("props fill attribute values and text once") {
		const Loaded loaded = load(R"(
<Component name="Tip" props="tip"><View tooltip="Says $tip"><Label>$tip!</Label></View></Component>
<View id="root"><Tip tip="hi"/><Tip/></View>)");
		REQUIRE(loaded.root != nullptr);
		CHECK_MESSAGE(loaded.log.empty(), loaded.log);
		CHECK(loaded.root->get_children()[0]->get_tooltip() == "Says hi");
		CHECK(loaded.root->get_children()[1]->get_tooltip() == "Says ");
		const auto *label = dynamic_cast<const Label *>(loaded.root->get_children()[0]->get_children()[0].get());
		REQUIRE(label != nullptr);
		CHECK(label->get_text() == "hi!");
	}

	TEST_CASE("text between the tags is the prop text") {
		const Loaded loaded =
			load(R"(<Component name="Title" props="text"><Label>$text</Label></Component><Title>Scene</Title>)");
		CHECK_MESSAGE(loaded.log.empty(), loaded.log);
		const auto *label = dynamic_cast<const Label *>(loaded.root.get());
		REQUIRE(label != nullptr);
		CHECK(label->get_text() == "Scene");
	}

	TEST_CASE("a double dollar is a dollar") {
		const Loaded loaded = load(R"(<Component name="Price"><Label>$$5</Label></Component><Price/>)");
		const auto *label = dynamic_cast<const Label *>(loaded.root.get());
		REQUIRE(label != nullptr);
		CHECK(label->get_text() == "$5");
	}

	TEST_CASE("props pass down through nested components and slots") {
		const Loaded loaded = load(R"(
<Component name="Inner" props="label"><View><Label>$label</Label><Slot/></View></Component>
<Component name="Outer" props="title"><Inner label="$title"><Label>in $title</Label></Inner></Component>
<Outer title="Hi"/>)");
		REQUIRE(loaded.root != nullptr);
		CHECK_MESSAGE(loaded.log.empty(), loaded.log);
		REQUIRE(loaded.root->get_children().size() == 2);
		CHECK(dynamic_cast<const Label *>(loaded.root->get_children()[0].get())->get_text() == "Hi");
		CHECK(dynamic_cast<const Label *>(loaded.root->get_children()[1].get())->get_text() == "in Hi");
	}

	TEST_CASE("an undeclared prop is reported") {
		CHECK(load(R"(<Component name="U"><Label>$nope</Label></Component><U/>)")
				  .log.find("'U' has no prop named 'nope'") != std::string::npos);
	}

	TEST_CASE("markup outside a component is never substituted") {
		const Loaded loaded = load(R"(<Label>$5 and $name</Label>)");
		CHECK_MESSAGE(loaded.log.empty(), loaded.log);
		CHECK(dynamic_cast<const Label *>(loaded.root.get())->get_text() == "$5 and $name");
	}
}

namespace {

constexpr const char *K_FILE = "/markup-tests/rows.aqlayout";

struct Card : Behaviour {
	bool defined = false;
	View *found = nullptr;
	void define_behaviour() override {
		defined = true;
		found = get("x");
	}
};

struct OtherBehaviour : Behaviour {};

struct Maker : Behaviour {
	View *made = nullptr;
	Unique<View> detached;
	void define_behaviour() override {
		made = create("Row", root(), { .attributes = { { "label", "made" } } });
		detached = create_detached("Row", { .attributes = { { "label", "detached" } } });
	}
};

std::vector<std::string> texts(const View &view) {
	std::vector<std::string> out;
	for (const auto &child : view.get_children()) {
		if (const auto *label = dynamic_cast<const Label *>(child.get())) {
			out.push_back(label->get_text());
		}
	}
	return out;
}

bool has_class(const View &view, std::string_view name) {
	return std::ranges::find(view.get_classes(), name) != view.get_classes().end();
}

constexpr const char *K_ROW =
	R"(<Component name="Row" props="label"><View class="row"><Label class="row-label">$label</Label></View></Component>)";

}

TEST_SUITE("Components from code") {
	TEST_CASE("code gives a component its props and extra classes") {
		MarkupFiles files;
		files.write(K_FILE, K_ROW);
		LogCapture capture;
		LayoutLoader loader;
		View parent;
		View *row =
			loader.create(K_FILE, "Row", parent, { .attributes = { { "label", "Speed" }, { "class", "extra" } } });
		REQUIRE(row != nullptr);
		CHECK_MESSAGE(capture.stream.str().empty(), capture.stream.str());
		CHECK(row->get_classes() == std::vector<std::string>{ "row", "extra" });
		CHECK(texts(*row) == std::vector<std::string>{ "Speed" });
	}

	TEST_CASE("prop values from code show as written") {
		MarkupFiles files;
		files.write(K_FILE, K_ROW);
		LogCapture capture;
		LayoutLoader loader;
		View parent;
		const std::string written = "Cost $x $$ & <b> \"q\"";
		View *row = loader.create(K_FILE, "Row", parent, { .attributes = { { "label", written } } });
		REQUIRE(row != nullptr);
		CHECK_MESSAGE(capture.stream.str().empty(), capture.stream.str());
		CHECK(texts(*row) == std::vector<std::string>{ written });
	}

	TEST_CASE("a text attribute from code reaches a text prop") {
		MarkupFiles files;
		files.write(K_FILE, R"(<Component name="T" props="text"><View><Label>$text</Label></View></Component>)");
		LayoutLoader loader;
		View parent;
		View *made = loader.create(K_FILE, "T", parent, { .attributes = { { "text", "hello" } } });
		REQUIRE(made != nullptr);
		CHECK(texts(*made) == std::vector<std::string>{ "hello" });
	}

	TEST_CASE("code-created children go into the slot in order") {
		MarkupFiles files;
		files.write(K_FILE,
					R"(<Component name="Box"><View><Label>head</Label><Slot/><Label>tail</Label></View></Component>)");
		LayoutLoader loader;
		View parent;
		ComponentArgs args;
		args.children.push_back(std::make_unique<Label>("a"));
		args.children.push_back(std::make_unique<Label>("b"));
		View *box = loader.create(K_FILE, "Box", parent, std::move(args));
		REQUIRE(box != nullptr);
		CHECK(texts(*box) == std::vector<std::string>{ "head", "a", "b", "tail" });
	}

	TEST_CASE("slot classes go on every child placed there") {
		MarkupFiles files;
		files.write(K_FILE, R"(<Component name="Field"><View><Slot class="value wide"/></View></Component>
<View><Field><View id="m"/></Field></View>)");
		LogCapture capture;
		LayoutLoader loader;
		const Unique<View> root = loader.load_file(K_FILE);
		REQUIRE(root != nullptr);
		CHECK_MESSAGE(capture.stream.str().empty(), capture.stream.str());
		const View &markup_child = *root->get_children().front()->get_children().front();
		CHECK(markup_child.get_id() == "m");
		CHECK(has_class(markup_child, "value"));
		CHECK(has_class(markup_child, "wide"));

		View parent;
		ComponentArgs args;
		auto child = std::make_unique<View>();
		View *code_child = child.get();
		args.children.push_back(std::move(child));
		REQUIRE(loader.create(K_FILE, "Field", parent, std::move(args)) != nullptr);
		CHECK(has_class(*code_child, "value"));
		CHECK(has_class(*code_child, "wide"));
	}

	TEST_CASE("a slot takes only a class") {
		LogCapture capture;
		LayoutLoader loader;
		const Unique<View> root =
			loader.load_string(R"(<Component name="F"><View><Slot id="s"/></View></Component><F/>)");
		REQUIRE(root != nullptr);
		CHECK_MESSAGE(capture.stream.str().find("<Slot/> only takes 'class', 'id' is ignored") != std::string::npos,
					  capture.stream.str());
	}

	TEST_CASE("a typed create returns the behaviour, or null and adds nothing") {
		MarkupFiles files;
		files.write(K_FILE, R"(<Component name="Card" behaviour="Card"><View><Slot/></View></Component>)");
		LayoutLoader loader;
		const Aquila::Foundation::Registration card = loader.behaviours().add<Card>("Card");
		View parent;
		LogCapture capture;
		CHECK(loader.create<Card>(K_FILE, "Card", parent) != nullptr);
		CHECK(loader.create<OtherBehaviour>(K_FILE, "Card", parent) == nullptr);
		CHECK(parent.get_children().size() == 1);
	}

	TEST_CASE("an instance's behaviour doesn't find children code gave it") {
		MarkupFiles files;
		files.write(K_FILE, R"(<Component name="Card" behaviour="Card"><View><Slot/></View></Component>)");
		LayoutLoader loader;
		const Aquila::Foundation::Registration registration = loader.behaviours().add<Card>("Card");
		View parent;
		ComponentArgs args;
		auto child = std::make_unique<View>();
		child->set_id("x");
		args.children.push_back(std::move(child));
		LogCapture capture;
		Card *card = loader.create<Card>(K_FILE, "Card", parent, std::move(args));
		REQUIRE(card != nullptr);
		CHECK(card->defined);
		CHECK(card->found == nullptr);
		CHECK(card->root().get_children().size() == 1);
	}

	TEST_CASE("children for a component without a slot are reported and dropped") {
		MarkupFiles files;
		files.write(K_FILE, R"(<Component name="Plain"><View/></Component>)");
		LayoutLoader loader;
		View parent;
		ComponentArgs args;
		args.children.push_back(std::make_unique<View>());
		LogCapture capture;
		View *plain = loader.create(K_FILE, "Plain", parent, std::move(args));
		REQUIRE(plain != nullptr);
		CHECK(plain->get_children().empty());
		CHECK_MESSAGE(capture.stream.str().find("has no <Slot/>") != std::string::npos, capture.stream.str());
	}

	TEST_CASE("a detached instance is owned by the caller and its behaviour has run") {
		MarkupFiles files;
		files.write(K_FILE, R"(<Component name="Card" behaviour="Card"><View/></Component>)");
		LayoutLoader loader;
		const Aquila::Foundation::Registration registration = loader.behaviours().add<Card>("Card");
		LogCapture capture;
		const Unique<View> detached = loader.create_detached(K_FILE, "Card");
		REQUIRE(detached != nullptr);
		CHECK(detached->get_parent() == nullptr);
		REQUIRE(detached->get_component_instance() != nullptr);
		const auto *card = dynamic_cast<const Card *>(detached->get_component_instance()->get_behaviour());
		REQUIRE(card != nullptr);
		CHECK(card->defined);
		CHECK(loader.create_detached(K_FILE, "Nope") == nullptr);
	}

	TEST_CASE("a behaviour creates its file's components with props") {
		MarkupFiles files;
		files.write(K_FILE, std::string(K_ROW) + R"(<Component name="Maker" behaviour="Maker"><View/></Component>)");
		LayoutLoader loader;
		const Aquila::Foundation::Registration registration = loader.behaviours().add<Maker>("Maker");
		View parent;
		auto *maker = loader.create<Maker>(K_FILE, "Maker", parent);
		REQUIRE(maker != nullptr);
		REQUIRE(maker->made != nullptr);
		CHECK(texts(*maker->made) == std::vector<std::string>{ "made" });
		REQUIRE(maker->detached != nullptr);
		CHECK(maker->detached->get_parent() == nullptr);
		CHECK(texts(*maker->detached) == std::vector<std::string>{ "detached" });
	}

	TEST_CASE("a behaviour creating components on every rebuild leaves the style set alone") {
		MarkupFiles files;
		files.write(K_FILE, std::string(K_ROW) + R"(<Component name="Maker" behaviour="Maker"><View/></Component>)");
		LayoutLoader loader;
		const Aquila::Foundation::Registration registration = loader.behaviours().add<Maker>("Maker");
		View parent;
		auto *maker = loader.create<Maker>(K_FILE, "Maker", parent);
		REQUIRE(maker != nullptr);
		const Uint64 revision = loader.styles().get_revision();
		for (int rebuild = 0; rebuild < 20; ++rebuild) {
			maker->root().clear_children();
			REQUIRE(maker->create("Row", maker->root(), { .attributes = { { "label", "again" } } }) != nullptr);
		}
		CHECK(texts(*maker->root().get_children().front()) == std::vector<std::string>{ "again" });
		CHECK(loader.styles().get_revision() == revision);
	}

	TEST_CASE("a relative file resolves against the UI folder, not the last file read") {
		MarkupFiles files;
		files.write(K_FILE, R"(<View/>)");
		LayoutLoader loader;
		REQUIRE(loader.load_file(K_FILE) != nullptr);
		View parent;
		LogCapture capture;
		CHECK(loader.create("Components/no_such_file", "X", parent) == nullptr);
		CHECK_MESSAGE(capture.stream.str().find("'/resources/Engine/UI/Components/no_such_file.aqlayout'") !=
						  std::string::npos,
					  capture.stream.str());
		CHECK(engine_component_file("property_grid") == "/resources/Engine/UI/Components/property_grid.aqlayout");
	}

	TEST_CASE("find_layout_loader needs a canvas with a loader") {
		CanvasSingletonsScope singletons;
		LayoutLoader loader;
		const View bare;
		CHECK(find_layout_loader(bare) == nullptr);
		auto canvas = std::make_unique<Canvas>(100, 100);
		View *inside = canvas->get_root()->add_child<View>();
		CHECK(find_layout_loader(*inside) == nullptr);
		canvas->set_layout_loader(&loader);
		CHECK(find_layout_loader(*inside) == &loader);
	}
}
