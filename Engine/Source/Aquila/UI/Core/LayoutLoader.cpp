#include "Aquila/Foundation/Macros.h"
#include "Aquila/UI/Core/LayoutLoader.h"
#include "Aquila/UI/Markup/MarkupParser.h"
#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"
#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/Checkbox.h"
#include "Aquila/UI/Widgets/Image.h"
#include "Aquila/UI/Widgets/IconLabel.h"
#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/Slider.h"
#include "Aquila/UI/Widgets/Popup.h"
#include "Aquila/UI/Widgets/PopupMenu.h"
#include "Aquila/UI/Widgets/TextInput.h"
#include "Aquila/UI/Widgets/NumberInput.h"
#include "Aquila/UI/Widgets/OverlayToolbar.h"
#include "Aquila/UI/Widgets/DragFloat.h"
#include "Aquila/UI/Widgets/DragInt.h"
#include "Aquila/UI/Widgets/Toggle.h"
#include "Aquila/UI/Widgets/ScrollView.h"
#include "Aquila/UI/Widgets/Collapsible.h"
#include "Aquila/UI/Widgets/TabBar.h"
#include "Aquila/UI/Widgets/TabView.h"
#include "Aquila/UI/Widgets/Dropdown.h"
#include "Aquila/UI/Widgets/VecField.h"
#include "Aquila/UI/Widgets/PropertyGrid.h"
#include "Aquila/UI/Widgets/TreeView.h"
#include "Aquila/UI/Widgets/Separator.h"
#include "Aquila/UI/Widgets/ProgressBar.h"
#include "Aquila/UI/Widgets/Tooltip.h"
#include "Aquila/UI/Widgets/Menubar.h"
#include "Aquila/UI/Widgets/ListBox.h"
#include "Aquila/UI/Widgets/AssetSlot.h"
#include "Aquila/UI/Widgets/AssetCard.h"
#include "Aquila/UI/Widgets/DockSpace.h"
#include "Aquila/UI/Widgets/DockNode.h"
#include "Aquila/UI/Widgets/DockPanel.h"
#include "Aquila/UI/Style/StyleParser.h"
#include "Aquila/UI/Style/StyleParserHelper.h"

namespace Aquila::UI::Core {

namespace {

void apply_inline_style(UI::StyleProperties &props, std::string_view decls, const Markup::MarkupDocument &document,
						Markup::SourceLocation where) {
	while (!decls.empty()) {
		const auto semi = decls.find(';');
		const std::string_view decl =
			Foundation::trim((semi != std::string_view::npos) ? decls.substr(0, semi) : decls);
		decls = (semi != std::string_view::npos) ? decls.substr(semi + 1) : std::string_view{};
		if (decl.empty()) {
			continue;
		}

		const auto colon = decl.find(':');
		const std::string_view prop = colon == std::string_view::npos ? decl : Foundation::trim(decl.substr(0, colon));
		const std::string_view val =
			colon == std::string_view::npos ? std::string_view{} : Foundation::trim(decl.substr(colon + 1));
		if (prop.empty() || val.empty()) {
			AQUILA_LOG_ERROR("LayoutLoader: {}: style declaration '{}' needs 'property: value'",
							 document.location(where), decl);
			continue;
		}
		UI::StyleParser::apply_property(props, prop, val);
	}
}

bool is_built_in_attribute(std::string_view name) {
	return name == "id" || name == "class" || name == "font" || name == "style" || name == "src" || name == "uv" ||
		name == "tint";
}

} // namespace

LayoutLoader::LayoutLoader() {
	register_builtins();
}

void LayoutLoader::register_font(const std::string &name, Text::FontAtlas *font) {
	m_fonts[name] = font;
	if (m_default_font == nullptr) {
		m_default_font = font;
	}
}

void LayoutLoader::set_default_font(Text::FontAtlas *font) {
	m_default_font = font;
}

void LayoutLoader::register_widget(const std::string &type_name, WidgetFactory factory) {
	m_factories[type_name] = std::move(factory);
}

Text::FontAtlas *LayoutLoader::resolve_font(const std::string &name) const {
	auto it = m_fonts.find(name);
	return (it != m_fonts.end()) ? it->second : m_default_font;
}

void LayoutLoader::register_texture_cache(TextureCache *cache) {
	m_texture_cache = cache;
}

GFX::GfxTexture *LayoutLoader::resolve_texture(const std::string &path) const {
	if (m_texture_cache == nullptr) {
		AQUILA_LOG_ERROR("LayoutLoader: no TextureCache registered — call RegisterTextureCache() first");
		return nullptr;
	}
	return m_texture_cache->load(path);
}

Delegate<void()> LayoutLoader::resolve_command(const std::string &name) const {
	if (m_command_source == nullptr) {
		return {};
	}
	if (m_command_source->find(name) == nullptr) {
		AQUILA_LOG_WARNING("LayoutLoader: unknown command '{}'", name);
		return {};
	}
	return [cmds = m_command_source, name] { cmds->execute(name); };
}

CommandInfo LayoutLoader::query_command(std::string_view name) const {
	CommandInfo info;
	if (m_command_source == nullptr) {
		return info;
	}
	const Command *cmd = m_command_source->find(name);
	if (cmd == nullptr) {
		return info;
	}
	info.known = true;
	info.enabled = m_command_source->is_enabled(name);
	info.title = cmd->title;
	if (m_shortcut_source != nullptr) {
		info.shortcut = m_shortcut_source->get_label(name);
	}
	return info;
}

void LayoutLoader::set_command_source(const Commands *commands, const Shortcuts *shortcuts) {
	if (commands != nullptr) {
		m_command_source = commands;
	}

	if (shortcuts != nullptr) {
		m_shortcut_source = shortcuts;
	}
}

Unique<View> LayoutLoader::create_widget(const std::string &type, std::string_view text, Text::FontAtlas *font) const {
	auto it = m_factories.find(type);
	if (it == m_factories.end()) {
		return nullptr;
	}
	return it->second(text, font);
}

Unique<View> LayoutLoader::load_file(const std::string &path) {
	std::string resolved = path;
	if (!path.empty() && path.front() != '/' && !m_current_dir.empty()) {
		resolved = m_current_dir + "/" + path;
	}

	if (std::ranges::find(m_include_stack, resolved) != m_include_stack.end()) {
		AQUILA_LOG_ERROR("LayoutLoader: include cycle detected at '{}'", resolved);
		return nullptr;
	}

	const std::string src = Platform::Filesystem::VirtualFileSystem::get()->read_text_file(resolved);
	if (src.empty()) {
		AQUILA_LOG_ERROR("LayoutLoader: cannot open '{}'", resolved);
		return nullptr;
	}

	const std::string prev_dir = m_current_dir;
	const auto slash = resolved.find_last_of('/');
	m_current_dir = (slash != std::string::npos) ? resolved.substr(0, slash) : std::string();
	m_include_stack.push_back(resolved);

	Unique<View> result = load_string(src);

	m_include_stack.pop_back();
	m_current_dir = prev_dir;
	return result;
}

struct LayoutLoader::BuildScope {
	const Markup::MarkupDocument *document = nullptr;
	const Markup::ComponentDefinition *definition = nullptr;
	const Markup::MarkupNode *instance = nullptr;
	const BuildScope *outer = nullptr;
	std::map<std::string, std::string, std::less<>> props{};

	[[nodiscard]] bool takes_prop(std::string_view name) const {
		return definition != nullptr && std::ranges::find(definition->props, name) != definition->props.end();
	}

	[[nodiscard]] std::string substitute(std::string_view value, Markup::SourceLocation where) const {
		if (definition == nullptr || value.find('$') == std::string_view::npos) {
			return std::string(value);
		}
		std::string out;
		Usize i = 0;
		while (i < value.size()) {
			if (value[i] != '$') {
				out.push_back(value[i++]);
				continue;
			}
			if (i + 1 < value.size() && value[i + 1] == '$') {
				out.push_back('$');
				i += 2;
				continue;
			}
			Usize end = i + 1;
			if (end < value.size() &&
				(std::isalpha(static_cast<unsigned char>(value[end])) != 0 || value[end] == '_')) {
				while (end < value.size() &&
					   (std::isalnum(static_cast<unsigned char>(value[end])) != 0 || value[end] == '_' ||
						value[end] == '-')) {
					++end;
				}
			}
			if (end == i + 1) {
				out.push_back('$');
				++i;
				continue;
			}
			const std::string_view name = value.substr(i + 1, end - i - 1);
			if (takes_prop(name)) {
				if (const auto it = props.find(name); it != props.end()) {
					out += it->second;
				}
			} else {
				AQUILA_LOG_ERROR("LayoutLoader: {}: '{}' has no prop named '{}'", document->location(where),
								 definition->name, name);
			}
			i = end;
		}
		return out;
	}

	[[nodiscard]] bool is_building(const Markup::ComponentDefinition &other) const {
		for (const BuildScope *scope = this; scope != nullptr; scope = scope->outer) {
			if (scope->definition == &other) {
				return true;
			}
		}
		return false;
	}
};

Unique<View> LayoutLoader::load_string(std::string_view xml) {
	std::string source = m_include_stack.empty() ? std::string("<string>") : m_include_stack.back();
	const Markup::MarkupDocument document = Markup::parse_markup(xml, std::move(source));
	for (const Markup::ComponentDefinition &definition : document.components) {
		if (m_factories.contains(definition.name)) {
			AQUILA_LOG_ERROR("LayoutLoader: {}: <Component '{}'> has the name of a widget",
							 document.location(definition.where), definition.name);
		}
	}
	if (!document.root) {
		return nullptr;
	}
	const BuildScope scope{ .document = &document };
	return build_element(*document.root, scope);
}

Unique<View> LayoutLoader::build_element(const Markup::MarkupNode &node, const BuildScope &scope) {
	const Markup::MarkupDocument &document = *scope.document;
	if (node.tag == "Include") {
		if (!node.children.empty()) {
			AQUILA_LOG_WARNING("LayoutLoader: {}: the children of <Include> are ignored",
							   document.location(node.where));
		}
		const Markup::MarkupAttribute *src = node.find("src");
		const std::string path = src != nullptr ? scope.substitute(src->value, src->value_where) : std::string();
		if (path.empty()) {
			AQUILA_LOG_ERROR("LayoutLoader: {}: <Include> is missing a 'src' attribute", document.location(node.where));
			return nullptr;
		}
		return load_file(path);
	}

	if (node.tag == "Slot") {
		AQUILA_LOG_ERROR("LayoutLoader: {}: <Slot/> only works inside a <Component>, as one of its elements",
						 document.location(node.where));
		return nullptr;
	}

	Text::FontAtlas *font = resolve_font("default");
	if (const Markup::MarkupAttribute *font_name = node.find("font")) {
		font = resolve_font(scope.substitute(font_name->value, font_name->value_where));
	}

	Unique<View> view = create_widget(node.tag, "", font);
	if (!view) {
		if (const Markup::ComponentDefinition *definition = document.find_component(node.tag)) {
			return build_component(*definition, node, scope);
		}
		AQUILA_LOG_ERROR("LayoutLoader: {}: unknown widget type '{}'", document.location(node.where), node.tag);
		return nullptr;
	}

	UI::StyleProperties props;
	for (const Markup::MarkupAttribute &attribute : node.attributes) {
		if (attribute.name == "id") {
			view->set_id(scope.substitute(attribute.value, attribute.value_where));
		} else if (attribute.name == "class") {
			std::istringstream iss(scope.substitute(attribute.value, attribute.value_where));
			std::string cls;
			while (iss >> cls) {
				view->add_class(cls);
			}
		} else if (attribute.name == "style") {
			apply_inline_style(props, scope.substitute(attribute.value, attribute.value_where), document,
							   attribute.value_where);
		}
	}
	view->merge_style(props);

	if (font != nullptr) {
		view->set_font(font);
	}

	for (const std::string_view name : { "tint", "src", "uv" }) {
		const Markup::MarkupAttribute *attribute = node.find(name);
		if (attribute == nullptr) {
			continue;
		}
		const std::string value = scope.substitute(attribute->value, attribute->value_where);
		if (!value.empty()) {
			view->apply_xml_attribute(name, value, this);
		}
	}

	for (const Markup::MarkupAttribute &attribute : node.attributes) {
		if (!is_built_in_attribute(attribute.name)) {
			view->apply_xml_attribute(attribute.name, scope.substitute(attribute.value, attribute.value_where), this);
		}
	}

	build_children(*view, node, scope, font);
	view->on_xml_loaded();
	return view;
}

Unique<View> LayoutLoader::build_component(const Markup::ComponentDefinition &definition,
										   const Markup::MarkupNode &instance, const BuildScope &scope) {
	const Markup::MarkupDocument &document = *scope.document;
	const std::string where = document.location(instance.where);
	if (scope.is_building(definition)) {
		AQUILA_LOG_ERROR("LayoutLoader: {}: '{}' uses itself", where, definition.name);
		return nullptr;
	}
	if (definition.root.tag == "Slot") {
		AQUILA_LOG_ERROR("LayoutLoader: {}: '{}' is only a <Slot/>, a component needs an element around it", where,
						 definition.name);
		return nullptr;
	}
	BuildScope inner{ .document = scope.document, .definition = &definition, .instance = &instance, .outer = &scope };
	const bool text_is_prop = inner.takes_prop("text");
	const bool has_children = std::ranges::any_of(instance.children, [text_is_prop](const Markup::MarkupNode &child) {
		return !text_is_prop || !child.is_text();
	});
	if (!definition.has_slot && has_children) {
		AQUILA_LOG_WARNING("LayoutLoader: {}: '{}' has no <Slot/>, its children are dropped", where, definition.name);
	}

	for (const Markup::MarkupAttribute &attribute : instance.attributes) {
		if (inner.takes_prop(attribute.name)) {
			inner.props[attribute.name] = scope.substitute(attribute.value, attribute.value_where);
		} else if (attribute.name != "id" && attribute.name != "class" && attribute.name != "style") {
			AQUILA_LOG_WARNING("LayoutLoader: {}: '{}' doesn't take the attribute '{}'",
							   document.location(attribute.where), definition.name, attribute.name);
		}
	}
	if (text_is_prop) {
		std::string text;
		for (const Markup::MarkupNode &child : instance.children) {
			if (child.is_text()) {
				text += (text.empty() ? "" : " ") + scope.substitute(child.text, child.where);
			}
		}
		inner.props["text"] = std::move(text);
	}

	Unique<View> root = build_element(definition.root, inner);
	if (!root) {
		return nullptr;
	}

	UI::StyleProperties props;
	for (const Markup::MarkupAttribute &attribute : instance.attributes) {
		if (attribute.name == "id") {
			root->set_id(scope.substitute(attribute.value, attribute.value_where));
		} else if (attribute.name == "class") {
			std::istringstream iss(scope.substitute(attribute.value, attribute.value_where));
			std::string cls;
			while (iss >> cls) {
				root->add_class(cls);
			}
		} else if (attribute.name == "style") {
			apply_inline_style(props, scope.substitute(attribute.value, attribute.value_where), document,
							   attribute.value_where);
		}
	}
	root->merge_style(props);
	return root;
}

void LayoutLoader::build_children(View &view, const Markup::MarkupNode &node, const BuildScope &scope,
								  Text::FontAtlas *font, bool skip_text) {
	for (const Markup::MarkupNode &child : node.children) {
		if (child.is_text()) {
			if (skip_text) {
				continue;
			}
			view.apply_xml_text_content(scope.substitute(child.text, child.where));
			if (font != nullptr) {
				view.set_font(font);
			}
			continue;
		}
		if (child.tag == "Slot" && scope.definition != nullptr) {
			if (scope.instance != nullptr && scope.outer != nullptr) {
				build_children(view, *scope.instance, *scope.outer, font, scope.takes_prop("text"));
			}
			continue;
		}
		if (Unique<View> built = build_element(child, scope)) {
			view.add_child(std::move(built));
		}
	}
}

void LayoutLoader::register_builtins() {
	m_factories["Label"] = [](std::string_view text, Text::FontAtlas *font) -> Unique<View> {
		return std::make_unique<Label>(std::string(text), font);
	};
	m_factories["Collapsible"] = [](std::string_view text, Text::FontAtlas *) -> Unique<View> {
		return std::make_unique<Collapsible>(std::string(text));
	};

	Register<View>("View");
	Register<Button>("Button");
	Register<Image>("Image");
	Register<IconLabel>("IconLabel");
	Register<Checkbox>("Checkbox");
	Register<Slider>("Slider");
	Register<TextInput>("TextInput");
	Register<Popup>("Popup");
	Register<PopupMenu>("PopupMenu");
	Register<NumberInput>("NumberInput");
	Register<DragFloat>("DragFloat");
	Register<DragInt>("DragInt");
	Register<Toggle>("Toggle");
	Register<ScrollView>("ScrollView");
	Register<TabBar>("TabBar");
	Register<TabView>("TabView");
	Register<Dropdown>("Dropdown");
	Register<Vec2Field>("Vec2Field");
	Register<Vec3Field>("Vec3Field");
	Register<Vec4Field>("Vec4Field");
	Register<PropertyGrid>("PropertyGrid");
	Register<TreeView>("TreeView");
	Register<Separator>("Separator");
	Register<ProgressBar>("ProgressBar");
	Register<Tooltip>("Tooltip");
	Register<MenuBar>("MenuBar");
	Register<ListBox>("ListBox");
	Register<AssetSlot>("AssetSlot");
	Register<AssetCard>("AssetCard");
	Register<DockSpace>("DockSpace");
	Register<DockNode>("DockNode");
	Register<DockPanel>("DockPanel");
	Register<OverlayToolbar>("OverlayToolbar");
}

} // namespace Aquila::UI::Core
