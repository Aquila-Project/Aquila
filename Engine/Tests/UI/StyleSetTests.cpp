#include <doctest.h>

#include "Aquila/UI/Core/Canvas.h"
#include "Aquila/UI/Core/LayoutLoader.h"
#include "Aquila/UI/Core/View.h"
#include "Aquila/UI/Style/StyleParser.h"
#include "Aquila/UI/Style/StyleSet.h"
#include "Aquila/UI/Style/StyleSheet.h"
#include "CanvasTestSupport.h"
#include "VfsTestSupport.h"

using namespace Aquila::UI;
using namespace Aquila::UI::Core;

TEST_SUITE("Style set") {
	TEST_CASE("the order is base, components, windows, whatever loaded first") {
		MarkupFiles files;
		for (const char *name : { "c1", "c2", "w1", "w2" }) {
			files.write(std::string("/markup-tests/") + name + ".aqstyle", ".x {}");
		}
		files.write("/markup-tests/c1.aqlayout", R"(<Component name="C1"><View/></Component>)");
		files.write("/markup-tests/c2.aqlayout", R"(<Import src="c1"/><Component name="C2"><View/></Component>)");
		files.write("/markup-tests/w2.aqlayout", R"(<Import src="c2"/><View/>)");
		files.write("/markup-tests/w1.aqlayout", R"(<Import src="c1"/><View/>)");
		LayoutLoader loader;
		loader.styles().set_base({ "/markup-tests/base.aqstyle" });
		loader.load_file("/markup-tests/w2.aqlayout");
		loader.load_file("/markup-tests/w1.aqlayout");
		CHECK(loader.styles().get_paths() ==
			  std::vector<std::string>{ "/markup-tests/base.aqstyle", "/markup-tests/c1.aqstyle",
										"/markup-tests/c2.aqstyle", "/markup-tests/w1.aqstyle",
										"/markup-tests/w2.aqstyle" });
	}

	TEST_CASE("a window's class beats a component's rule whichever window loaded first") {
		CanvasSingletonsScope singletons;
		MarkupFiles files;
		files.write("/markup-tests/card.aqlayout", R"(<Component name="Card"><View class="card"/></Component>)");
		files.write("/markup-tests/card.aqstyle", ".card { width: 10px; }");
		files.write("/markup-tests/a.aqlayout", R"(<Import src="card"/><View><Card id="c" class="wide"/></View>)");
		files.write("/markup-tests/a.aqstyle", ".wide { width: 20px; }");
		LayoutLoader loader;
		Canvas canvas(200, 200);
		canvas.set_layout_loader(&loader);
		View *root = loader.build_file("/markup-tests/a.aqlayout", *canvas.get_root());
		REQUIRE(root != nullptr);
		canvas.update(0.F);
		canvas.compute();
		REQUIRE(root->find_by_id("c") != nullptr);
		CHECK(root->find_by_id("c")->get_layout_rect().size.x == doctest::Approx(20.F));
	}

	TEST_CASE("@import loads a file first and once") {
		MarkupFiles files;
		files.write("/markup-tests/tokens.aqstyle", ":root { --w: 5px; } .t { width: 1px; }");
		files.write("/markup-tests/a.aqstyle", "@import \"tokens.aqstyle\";\n.a { width: var(--w); }");
		files.write("/markup-tests/b.aqstyle", "@import \"/markup-tests/tokens.aqstyle\";\n.b { width: var(--w); }");
		StyleSheet tokens_only;
		StyleParser::load_files(std::vector<std::string>{ "/markup-tests/tokens.aqstyle" }, tokens_only);
		StyleSheet sheet;
		LogCapture capture;
		CHECK(StyleParser::load_files(std::vector<std::string>{ "/markup-tests/a.aqstyle", "/markup-tests/b.aqstyle" },
									  sheet));
		CHECK_MESSAGE(capture.stream.str().empty(), capture.stream.str());
		CHECK(sheet.get_rule_count() == tokens_only.get_rule_count() + 2);
	}

	TEST_CASE("@import problems are reported") {
		MarkupFiles files;
		files.write("/markup-tests/x.aqstyle", "@import \"y.aqstyle\";\n.x { width: 1px; }");
		files.write("/markup-tests/y.aqstyle", "@import \"x.aqstyle\";\n.y { width: 1px; }");
		files.write("/markup-tests/late.aqstyle", ".z { width: 1px; }\n@import \"x.aqstyle\";");
		StyleSheet sheet;
		LogCapture capture;
		StyleParser::load_files(std::vector<std::string>{ "/markup-tests/x.aqstyle" }, sheet);
		StyleParser::load_files(std::vector<std::string>{ "/markup-tests/late.aqstyle" }, sheet);
		StyleParser::LoadString("@import \"/markup-tests/x.aqstyle\";\n.s { width: 1px; }", sheet);
		const std::string log = capture.stream.str();
		CHECK(log.find("import cycle") != std::string::npos);
		CHECK(log.find("must come before the rules") != std::string::npos);
		CHECK(log.find("only works in files") != std::string::npos);
	}

	TEST_CASE("a canvas reloads once per change") {
		CanvasSingletonsScope singletons;
		MarkupFiles files;
		files.write("/markup-tests/base.aqstyle", ".a { width: 1px; } .b { width: 1px; }");
		files.write("/markup-tests/w.aqlayout", R"(<View/>)");
		files.write("/markup-tests/w.aqstyle", ".c { width: 1px; }");
		LayoutLoader loader;
		loader.styles().set_base({ "/markup-tests/base.aqstyle" });
		Canvas canvas(100, 100);
		canvas.set_layout_loader(&loader);
		canvas.update(0.F);
		CHECK(canvas.get_style_sheet().get_rule_count() == 2);
		const Uint64 revision = loader.styles().get_revision();
		canvas.update(0.F);
		CHECK(loader.styles().get_revision() == revision);
		loader.load_file("/markup-tests/w.aqlayout");
		CHECK(loader.styles().get_revision() != revision);
		canvas.update(0.F);
		CHECK(canvas.get_style_sheet().get_rule_count() == 3);
		files.write("/markup-tests/w.aqstyle", ".c { width: 1px; } .d { width: 1px; }");
		loader.styles().touch();
		canvas.update(0.F);
		CHECK(canvas.get_style_sheet().get_rule_count() == 4);
	}
}
