#include <doctest.h>

#include "Aquila/Foundation/Registration.h"
#include "Aquila/Foundation/Signal.h"
#include "Aquila/UI/Core/Behaviour.h"
#include "Aquila/UI/Core/BehaviourRegistry.h"
#include "Aquila/UI/Core/ComponentInstance.h"
#include "Aquila/UI/Core/LayoutLoader.h"
#include "Aquila/UI/Core/View.h"
#include "Aquila/UI/Widgets/Clickable.h"
#include "Aquila/UI/Widgets/Label.h"
#include "VfsTestSupport.h"

using namespace Aquila::UI::Core;
using Aquila::Foundation::Registration;

namespace {

struct Loaded {
	Unique<View> root;
	std::string log;
};

Loaded load_with(LayoutLoader &loader, std::string_view xml) {
	LogCapture capture;
	Unique<View> root = loader.load_string(xml);
	return { std::move(root), capture.stream.str() };
}

template <typename T> T *behaviour_of(const View *view) {
	return view != nullptr && view->get_component_instance() != nullptr
		? dynamic_cast<T *>(view->get_component_instance()->get_behaviour())
		: nullptr;
}

struct Card : Behaviour {
	Label *title = nullptr;
	void define_behaviour() override {
		title = get<Label>("title");
		order().push_back("card");
	}
	~Card() override { ++destroyed(); }
	static std::vector<std::string> &order() {
		static std::vector<std::string> v;
		return v;
	}
	static int &destroyed() {
		static int n = 0;
		return n;
	}
};

struct Outer : Behaviour {
	Label *inner_title = nullptr;
	Card *card = nullptr;
	void define_behaviour() override {
		inner_title = get<Label>("title");
		card = get_component<Card>("card");
		Card::order().push_back("outer");
	}
};

struct Probe : Behaviour {
	View *found = nullptr;
	void define_behaviour() override { found = get(attribute("find")); }
};

struct RemovesSibling : Behaviour {
	View *sibling = nullptr;
	~RemovesSibling() override {
		if (sibling != nullptr) {
			sibling->get_parent()->remove_child(sibling);
		}
	}
};

struct Listener : Behaviour {
	Signal<void()> *signal = nullptr;
	int *calls = nullptr;
	void connect() {
		listen(*signal, [this] { ++*calls; });
	}
};

struct RootClickListener : Behaviour {
	int clicks = 0;
	void define_behaviour() override {
		if (auto *clickable = dynamic_cast<Clickable *>(&root())) {
			listen(clickable->on_click, [this] { ++clicks; });
		}
	}
};
}

TEST_SUITE("Behaviours") {
	TEST_CASE("a behaviour finds its own elements and ids repeat across instances") {
		LayoutLoader loader;
		const Registration card = loader.behaviours().add<Card>("Card");
		const Loaded loaded = load_with(loader, R"(
<Component name="Card" behaviour="Card"><View><Label id="title"/></View></Component>
<View><Card/><Card/></View>)");
		REQUIRE(loaded.root != nullptr);
		CHECK_MESSAGE(loaded.log.empty(), loaded.log);
		auto *first = behaviour_of<Card>(loaded.root->get_children()[0].get());
		auto *second = behaviour_of<Card>(loaded.root->get_children()[1].get());
		REQUIRE(first != nullptr);
		REQUIRE(second != nullptr);
		CHECK(first->title != nullptr);
		CHECK(second->title != nullptr);
		CHECK(first->title != second->title);
	}

	TEST_CASE("a behaviour sees nested components' roots but not their insides") {
		Card::order().clear();
		LayoutLoader loader;
		const Registration card = loader.behaviours().add<Card>("Card");
		const Registration outer = loader.behaviours().add<Outer>("Outer");
		const Loaded loaded = load_with(loader, R"(
<Component name="Card" behaviour="Card"><View><Label id="title"/></View></Component>
<Component name="Box" behaviour="Outer"><View><Card id="card"/></View></Component>
<Box/>)");
		auto *box = behaviour_of<Outer>(loaded.root.get());
		REQUIRE(box != nullptr);
		CHECK(box->inner_title == nullptr);
		CHECK(loaded.log.find("no element 'title'") != std::string::npos);
		CHECK(box->card != nullptr);
		CHECK(Card::order() == std::vector<std::string>{ "card", "outer" });
	}

	TEST_CASE("slot children belong to whoever wrote them") {
		MarkupFiles files;
		files.write("/markup-tests/w.aqlayout", R"(
<Component name="Box" behaviour="Probe" props="find"><View><Slot/></View></Component>
<View><Box id="box" find="inner"><Label id="inner"/></Box></View>)");
		LayoutLoader loader;
		const Registration probe = loader.behaviours().add<Probe>("Probe");
		LogCapture capture;
		View parent;
		auto window = std::make_unique<Probe>();
		Probe *file_scope = window.get();
		View *root = loader.build_file("/markup-tests/w.aqlayout", parent, std::move(window));
		REQUIRE(root != nullptr);
		CHECK(file_scope->get<Label>("inner") != nullptr);
		REQUIRE(behaviour_of<Probe>(root->find_by_id("box")) != nullptr);
		CHECK(behaviour_of<Probe>(root->find_by_id("box"))->found == nullptr);
	}

	TEST_CASE("a behaviour dies with its root and its connections go with it") {
		LayoutLoader loader;
		const Registration listener = loader.behaviours().add<Listener>("Listener");
		const Loaded loaded = load_with(loader, R"(
<Component name="L" behaviour="Listener"><View/></Component><View><L/></View>)");
		Signal<void()> signal;
		int calls = 0;
		auto *behaviour = behaviour_of<Listener>(loaded.root->get_children()[0].get());
		REQUIRE(behaviour != nullptr);
		behaviour->signal = &signal;
		behaviour->calls = &calls;
		behaviour->connect();
		signal();
		CHECK(calls == 1);
		loaded.root->remove_child(loaded.root->get_children()[0].get());
		signal();
		CHECK(calls == 1);
	}

	TEST_CASE("a behaviour that removes a sibling while its parent dies doesn't crash") {
		LayoutLoader loader;
		const Registration removes = loader.behaviours().add<RemovesSibling>("RemovesSibling");
		Loaded loaded = load_with(loader, R"(
<Component name="A" behaviour="RemovesSibling"><View/></Component><View><A/><View id="b"/></View>)");
		auto *behaviour = behaviour_of<RemovesSibling>(loaded.root->get_children()[0].get());
		REQUIRE(behaviour != nullptr);
		behaviour->sibling = loaded.root->find_by_id("b");
		loaded.root.reset();
		CHECK(loaded.root == nullptr);
	}

	TEST_CASE("create builds components from the behaviour's file") {
		MarkupFiles files;
		files.write("/markup-tests/w.aqlayout", R"(
<Component name="Card" behaviour="Card"><View class="card"><Label id="title"/></View></Component>
<View><View id="list"/></View>)");
		LayoutLoader loader;
		const Registration card = loader.behaviours().add<Card>("Card");
		View parent;
		auto window = std::make_unique<Probe>();
		Probe *scope = window.get();
		loader.build_file("/markup-tests/w.aqlayout", parent, std::move(window));
		View *list = scope->get("list");
		REQUIRE(list != nullptr);
		CHECK(scope->create<Card>("Card", *list) != nullptr);
		CHECK(scope->create("Card", *list) != nullptr);
		LogCapture capture;
		CHECK(scope->create<Outer>("Card", *list) == nullptr);
		CHECK(list->get_children().size() == 2);
		CHECK(list->get_children()[0]->get_classes() == std::vector<std::string>{ "card" });
		CHECK_FALSE(capture.stream.str().empty());
	}

	TEST_CASE("attributes and text reach the behaviour") {
		LayoutLoader loader;
		const Registration probe = loader.behaviours().add<Probe>("Probe");
		const Loaded loaded =
			load_with(loader, R"(<Component name="P" behaviour="Probe"><View/></Component><P heading="Hi">Body</P>)");
		CHECK_MESSAGE(loaded.log.empty(), loaded.log);
		auto *behaviour = behaviour_of<Probe>(loaded.root.get());
		REQUIRE(behaviour != nullptr);
		CHECK(behaviour->attribute("heading") == "Hi");
		CHECK(behaviour->attribute("text") == "Body");
		CHECK(behaviour->attribute("missing").empty());
	}

	TEST_CASE("behaviours stack on a component rooted in another") {
		Card::order().clear();
		LayoutLoader loader;
		const Registration card = loader.behaviours().add<Card>("Card");
		const Registration outer = loader.behaviours().add<Outer>("Outer");
		const Loaded loaded = load_with(loader, R"(
<Component name="Card" behaviour="Card"><View><Label id="title"/></View></Component>
<Component name="Special" behaviour="Outer"><Card/></Component>
<Special/>)");
		REQUIRE(loaded.root != nullptr);
		REQUIRE(loaded.root->get_component_instance() != nullptr);
		CHECK(behaviour_of<Outer>(loaded.root.get()) != nullptr);
		REQUIRE(loaded.root->get_component_instance()->get_inner() != nullptr);
		CHECK(dynamic_cast<Card *>(loaded.root->get_component_instance()->get_inner()->get_behaviour()) != nullptr);
		CHECK(behaviour_of<Outer>(loaded.root.get())->inner_title == nullptr);
		CHECK(Card::order() == std::vector<std::string>{ "card", "outer" });
	}

	TEST_CASE("unknown behaviours, taken names and gone registrations") {
		LayoutLoader loader;
		Registration first = loader.behaviours().add<Card>("Card");
		LogCapture capture;
		const Registration second = loader.behaviours().add<Card>("Card");
		CHECK_FALSE(second.is_active());
		const Unique<View> root =
			loader.load_string(R"(<Component name="N" behaviour="Nope"><View id="n"/></Component><N/>)");
		REQUIRE(root != nullptr);
		CHECK(capture.stream.str().find("no behaviour named 'Nope'") != std::string::npos);
		first.reset();
		CHECK(loader.behaviours().create("Card") == nullptr);
	}

	TEST_CASE("LayoutLoader::create builds a component by file and name") {
		MarkupFiles files;
		files.write("/markup-tests/card.aqlayout", R"(<Component name="Card"><View class="card"/></Component>)");
		LayoutLoader loader;
		View parent;
		CHECK(loader.create("/markup-tests/card.aqlayout", "Card", parent) != nullptr);
		CHECK(parent.get_children().size() == 1);
	}

	TEST_CASE("a behaviour listening to its widget root goes away with it") {
		LayoutLoader loader;
		const Registration registration = loader.behaviours().add<RootClickListener>("RootClickListener");
		Loaded loaded = load_with(loader, R"(
<Component name="Tile" behaviour="RootClickListener"><Clickable/></Component>
<View><Tile id="tile"/></View>)");
		REQUIRE(loaded.root != nullptr);
		CHECK_MESSAGE(loaded.log.empty(), loaded.log);
		View *tile = loaded.root->get_children().front().get();
		auto *listener = behaviour_of<RootClickListener>(tile);
		REQUIRE(listener != nullptr);
		dynamic_cast<Clickable &>(*tile).on_click();
		CHECK(listener->clicks == 1);
		loaded.root.reset();
	}
}
