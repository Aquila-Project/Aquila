#include <doctest.h>

#include "Aquila/Foundation/Registration.h"
#include "Aquila/UI/Core/Behaviour.h"
#include "Aquila/UI/Core/Canvas.h"
#include "Aquila/UI/Core/LayoutLoader.h"
#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/TreeView.h"
#include "CanvasTestSupport.h"
#include "VfsTestSupport.h"

using namespace Aquila::UI::Core;

namespace {

constexpr const char *K_FILE = "/markup-tests/tree_rows.aqlayout";
constexpr const char *K_ROWS = R"(
<Component name="Eye" behaviour="Eye"><Button id="eye" class="eye"/></Component>
<Component name="Count"><Label class="count"/></Component>)";

class Eye final : public Behaviour {
  public:
	void define_behaviour() override { button = get<Button>("eye"); }
	Button *button = nullptr;
};

struct TreeCanvas {
	CanvasSingletonsScope singletons;
	MarkupFiles files;
	LayoutLoader loader;
	Aquila::Foundation::Registration eye = loader.behaviours().add<Eye>("Eye");
	Canvas canvas{ 400, 300 };
	TreeView *tree = nullptr;

	TreeCanvas() {
		files.write(K_FILE, K_ROWS);
		canvas.set_layout_loader(&loader);
		tree = canvas.get_root()->add_child<TreeView>();
		tree->set_row_component(K_FILE, "Eye");
	}
};

void check_one_eye(const TreeNode &node) {
	const Eye *eye = node.get_row_behaviour<Eye>();
	REQUIRE(eye != nullptr);
	CHECK(eye->button == node.get_row_component());
	CHECK(node.get_actions()->get_children().size() == 1);
}

}

TEST_SUITE("TreeView") {
	TEST_CASE("nodes added in a canvas get the row component") {
		TreeCanvas f;
		TreeNode *parent = f.tree->add_node("Cube");
		TreeNode *child = parent->add_child_node("Child");
		check_one_eye(*parent);
		check_one_eye(*child);
	}

	TEST_CASE("nodes built outside a canvas get it when they're added") {
		TreeCanvas f;
		LogCapture capture;
		auto node = std::make_unique<TreeNode>("Cube", *f.tree, 0);
		TreeNode *child = node->add_child_node("Child");
		CHECK(node->get_row_component() == nullptr);
		CHECK(child->get_row_component() == nullptr);

		auto *added = dynamic_cast<TreeNode *>(f.tree->add_child(std::move(node)));
		REQUIRE(added != nullptr);
		check_one_eye(*added);
		check_one_eye(*child);
		CHECK_MESSAGE(capture.stream.str().empty(), capture.stream.str());
	}

	TEST_CASE("a node moved to another parent keeps its row") {
		TreeCanvas f;
		TreeNode *from = f.tree->add_node("From");
		TreeNode *to = f.tree->add_node("To");
		TreeNode *moved = from->add_child_node("Moved");
		const View *row = moved->get_row_component();
		REQUIRE(row != nullptr);

		Unique<View> detached = from->get_children_container()->detach_child(moved);
		auto *again = dynamic_cast<TreeNode *>(to->add_child(std::move(detached)));
		REQUIRE(again != nullptr);
		CHECK(again->get_row_component() == row);
		check_one_eye(*again);
	}

	TEST_CASE("a node's own row component replaces the tree's") {
		TreeCanvas f;
		auto group = std::make_unique<TreeNode>("Lights", *f.tree, 0);
		group->set_row_component(K_FILE, "Count");
		auto *added = dynamic_cast<TreeNode *>(f.tree->add_child(std::move(group)));
		REQUIRE(added != nullptr);
		CHECK(dynamic_cast<Label *>(added->get_row_component()) != nullptr);
		CHECK(added->get_row_behaviour<Eye>() == nullptr);
	}

	TEST_CASE("a tree outside a canvas reports its rows and builds none") {
		CanvasSingletonsScope singletons;
		TreeView tree;
		tree.set_row_component(K_FILE, "Eye");
		LogCapture capture;
		TreeNode *node = tree.add_node("Cube");
		CHECK(node->get_row_component() == nullptr);
		CHECK(node->get_actions()->get_children().empty());
		CHECK_MESSAGE(capture.stream.str().find("TreeView: 'Cube' needs a canvas with a layout loader") !=
						  std::string::npos,
					  capture.stream.str());
	}
}
