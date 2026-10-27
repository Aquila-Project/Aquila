#include "Aquila/UI/DevTools/WidgetGalleryWindow.h"

#include "Aquila/GFX/GfxContext.h"
#include "Aquila/UI/Core/Canvas.h"
#include "Aquila/UI/Core/TextureCache.h"
#include "Aquila/UI/Core/View.h"

#include "Aquila/UI/Widgets/AssetCard.h"
#include "Aquila/UI/Widgets/AssetSlot.h"
#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/Checkbox.h"
#include "Aquila/UI/Widgets/Collapsible.h"
#include "Aquila/UI/Widgets/ColorPicker.h"
#include "Aquila/UI/Widgets/DragFloat.h"
#include "Aquila/UI/Widgets/DragInt.h"
#include "Aquila/UI/Widgets/Dropdown.h"
#include "Aquila/UI/Widgets/Image.h"
#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/ListBox.h"
#include "Aquila/UI/Widgets/NumberInput.h"
#include "Aquila/UI/Widgets/ProgressBar.h"
#include "Aquila/UI/Widgets/PropertyGrid.h"
#include "Aquila/UI/Widgets/ScrollView.h"
#include "Aquila/UI/Widgets/Separator.h"
#include "Aquila/UI/Widgets/Slider.h"
#include "Aquila/UI/Widgets/TabView.h"
#include "Aquila/UI/Widgets/TextInput.h"
#include "Aquila/UI/Widgets/Toggle.h"
#include "Aquila/UI/Widgets/Tooltip.h"
#include "Aquila/UI/Widgets/TreeView.h"
#include "Aquila/UI/Widgets/VecField.h"

namespace Aquila::UI::DevTools {

using namespace Aquila::UI::Core;

WidgetGalleryWindow::WidgetGalleryWindow() = default;
WidgetGalleryWindow::~WidgetGalleryWindow() = default;

View *WidgetGalleryWindow::add_group(View *host, const std::string &heading) {
	auto *group = host->add_child<View>();
	group->add_class("gallery-item");
	auto *label = group->add_child<Label>(heading);
	label->add_class("gallery-heading");
	return group;
}

void WidgetGalleryWindow::build(GFX::GfxContext &ctx, TextureCache *texture_cache, Uint32 width, Uint32 height,
								LayoutLoader &loader) {
	create_canvas(width, height, loader);

	auto *root = get_canvas()->get_root();
	root->set_id("gallery-root");
	root->add_class("gallery-root");

	auto *header = root->add_child<View>();
	header->add_class("gallery-header");
	auto *title = header->add_child<Label>(std::string("Widget Gallery"));
	title->add_class("gallery-title");

	auto *scroll = root->add_child<ScrollView>();
	scroll->add_class("gallery-scroll");
	auto *content = scroll->add_child<View>();
	content->add_class("gallery-content");

	m_tooltip = root->add_child<Tooltip>();

	add_group(content, "Button")->add_child<Button>(std::string("Click me"));

	add_group(content, "Label")->add_child<Label>(std::string("The quick brown fox"));

	add_group(content, "Checkbox")->add_child<Checkbox>(true);

	add_group(content, "Toggle")->add_child<Toggle>(true);

	auto *slider = add_group(content, "Slider")->add_child<Slider>();
	slider->set_range(0.F, 100.F);
	slider->set_value(40.F);

	auto *progress = add_group(content, "ProgressBar")->add_child<ProgressBar>();
	progress->set_value(0.65F);

	add_group(content, "TextInput")->add_child<TextInput>(std::string("Type here…"));

	add_group(content, "NumberInput")->add_child<NumberInput>();

	add_group(content, "DragFloat")->add_child<DragFloat>();

	add_group(content, "DragInt")->add_child<DragInt>();

	auto *dropdown = add_group(content, "Dropdown")->add_child<Dropdown>();
	dropdown->add_option("first", "First");
	dropdown->add_option("second", "Second");
	dropdown->add_option("third", "Third");

	add_group(content, "ColorPicker")->add_child<ColorPicker>(ctx, Vec4(0.8F, 0.4F, 0.2F, 1.F));

	add_group(content, "Vec3Field")->add_child<Vec3Field>();

	add_group(content, "Separator")->add_child<Separator>();

	auto *collapsible = add_group(content, "Collapsible")->add_child<Collapsible>(std::string("Expandable section"));
	collapsible->add_child<Label>(std::string("Hidden content revealed on expand"));

	auto *tabs = add_group(content, "TabView")->add_child<TabView>();
	tabs->add_class("gallery-tabview");
	tabs->add_tab("Tab One")->add_child<Label>(std::string("Contents of tab one"));
	tabs->add_tab("Tab Two")->add_child<Label>(std::string("Contents of tab two"));
	tabs->set_active_tab(0);

	auto *list_box = add_group(content, "ListBox")->add_child<ListBox>();
	list_box->add_class("gallery-listbox");
	list_box->add_item("a", "Alpha");
	list_box->add_item("b", "Bravo");
	list_box->add_item("c", "Charlie");

	auto *tree = add_group(content, "TreeView")->add_child<TreeView>();
	auto *tree_root = tree->add_node("Root");
	tree_root->add_child_node("Child A");
	tree_root->add_child_node("Child B");

	auto *grid = add_group(content, "PropertyGrid")->add_child<PropertyGrid>();
	grid->add_row<DragFloat>("Scale");
	grid->add_row<Checkbox>("Visible");

	if (texture_cache != nullptr) {
		auto *image =
			add_group(content, "Image")->add_child<Image>(texture_cache->load("Engine/UI/Icons/info.svg"), Vec4(1.F));
		image->add_class("gallery-image");
	}

	add_group(content, "AssetSlot")->add_child<AssetSlot>(std::string("texture"));

	auto *card = add_group(content, "AssetCard")->add_child<AssetCard>();
	card->set_asset(AssetPayload{ "textures/wood.png", "texture", "wood.png" });
	if (texture_cache != nullptr) {
		card->set_thumbnail(texture_cache->load("Engine/UI/Icons/info.svg"));
	}

	auto *tooltip_btn = add_group(content, "Tooltip")->add_child<Button>(std::string("Show tooltip"));
	Tooltip *tip = m_tooltip;
	tooltip_btn->on_click.connect([tip, tooltip_btn] {
		const Rect rect = tooltip_btn->get_absolute_rect();
		tip->show_at({ rect.position.x, rect.position.y + rect.size.y + 4.F }, "Hello from a Tooltip!");
	});

	get_canvas()->reload_styles();
}

} // namespace Aquila::UI::DevTools
