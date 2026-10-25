#include <doctest.h>

#include "Aquila/UI/Core/LayoutLoader.h"
#include "Aquila/UI/Core/View.h"
#include "Aquila/UI/Markup/MarkupPath.h"
#include "VfsTestSupport.h"

using namespace Aquila::UI::Core;
using Aquila::UI::Markup::resolve_markup_path;

TEST_SUITE("Imports") {
	TEST_CASE("paths resolve relative to the importing file") {
		CHECK(resolve_markup_path("/a/b", "../c", ".aqlayout") == "/a/c.aqlayout");
		CHECK(resolve_markup_path("/a", "./b/c", ".aqlayout") == "/a/b/c.aqlayout");
		CHECK(resolve_markup_path("/a", "/x/y.aqlayout", ".aqlayout") == "/x/y.aqlayout");
		CHECK(resolve_markup_path("/", "../x", ".aqlayout") == "/x.aqlayout");
	}

	TEST_CASE("an imported component is usable as a tag") {
		MarkupFiles files;
		files.write("/markup-tests/Components/card.aqlayout",
					R"(<Component name="Card"><View class="card"/></Component>)");
		files.write("/markup-tests/Windows/w.aqlayout",
					R"(<Import src="../Components/card"/><View><Card id="c"/></View>)");
		LogCapture capture;
		LayoutLoader loader;
		const Unique<View> root = loader.load_file("/markup-tests/Windows/w.aqlayout");
		REQUIRE(root != nullptr);
		CHECK_MESSAGE(capture.stream.str().empty(), capture.stream.str());
		REQUIRE(root->get_children().size() == 1);
		CHECK(root->get_children()[0]->get_classes() == std::vector<std::string>{ "card" });
	}

	TEST_CASE("imports are not transitive") {
		MarkupFiles files;
		files.write("/markup-tests/c.aqlayout", R"(<Component name="C"><View/></Component>)");
		files.write("/markup-tests/b.aqlayout", R"(<Import src="c"/><Component name="B"><C/></Component>)");
		files.write("/markup-tests/a.aqlayout", R"(<Import src="b"/><View><B/><C/></View>)");
		LogCapture capture;
		LayoutLoader loader;
		const Unique<View> root = loader.load_file("/markup-tests/a.aqlayout");
		REQUIRE(root != nullptr);
		CHECK(root->get_children().size() == 1);
		CHECK(capture.stream.str().find("unknown widget type 'C'") != std::string::npos);
	}

	TEST_CASE("a name visible twice, a missing import and a cycle are reported") {
		MarkupFiles files;
		files.write("/markup-tests/card.aqlayout", R"(<Component name="Card"><View/></Component>)");
		files.write("/markup-tests/twice.aqlayout",
					R"(<Import src="card"/><Component name="Card"><View/></Component><View><Card/></View>)");
		files.write("/markup-tests/missing.aqlayout", R"(<Import src="nowhere"/><View/>)");
		files.write("/markup-tests/x.aqlayout", R"(<Import src="y"/><View/>)");
		files.write("/markup-tests/y.aqlayout", R"(<Import src="x"/><Component name="Y"><View/></Component>)");
		LogCapture capture;
		LayoutLoader loader;
		CHECK(loader.load_file("/markup-tests/twice.aqlayout") != nullptr);
		CHECK(loader.load_file("/markup-tests/missing.aqlayout") != nullptr);
		CHECK(loader.load_file("/markup-tests/x.aqlayout") != nullptr);
		const std::string log = capture.stream.str();
		CHECK(log.find("is defined in both") != std::string::npos);
		CHECK(log.find("cannot open") != std::string::npos);
		CHECK(log.find("import cycle") != std::string::npos);
	}

	TEST_CASE("a changed file is read again") {
		MarkupFiles files;
		files.write("/markup-tests/w.aqlayout", R"(<View id="old"/>)");
		LayoutLoader loader;
		CHECK(loader.load_file("/markup-tests/w.aqlayout")->get_id() == "old");
		files.write("/markup-tests/w.aqlayout", R"(<View id="changed"/>)");
		CHECK(loader.load_file("/markup-tests/w.aqlayout")->get_id() == "changed");
	}
}
