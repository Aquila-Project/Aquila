#include "Aquila/Foundation/Macros.h"
#include "Aquila/UI/Core/LayoutLoader.h"
#include "Aquila/UI/Markup/MarkupParser.h"
#include "Aquila/UI/Markup/MarkupPath.h"
#include "Aquila/UI/Core/Canvas.h"
#include "Aquila/UI/Core/ComponentInstance.h"
#include "Aquila/UI/Core/UIResources.h"
#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"
#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/Checkbox.h"
#include "Aquila/UI/Widgets/Clickable.h"
#include "Aquila/UI/Widgets/Image.h"
#include "Aquila/UI/Widgets/IconLabel.h"
#include "Aquila/UI/Widgets/InspectorPage.h"
#include "Aquila/UI/Widgets/OverlayCheckRow.h"
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
#include "Aquila/UI/Widgets/SelectableTextView.h"
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

std::string resolve_layout_path(const std::string &current_dir, const std::string &path) {
	if (path.starts_with('/')) {
		return Markup::resolve_markup_path({}, path, {});
	}
	return path.empty() || current_dir.empty() ? path : Markup::resolve_markup_path(current_dir, path, {});
}

void check_slots(const Markup::MarkupDocument &document, const Markup::MarkupNode &node) {
	if (node.tag == "Slot") {
		for (const Markup::MarkupAttribute &attribute : node.attributes) {
			if (attribute.name != "class") {
				AQUILA_LOG_WARNING("LayoutLoader: {}: <Slot/> only takes 'class', '{}' is ignored",
								   document.location(attribute.where), attribute.name);
			}
		}
	}
	for (const Markup::MarkupNode &child : node.children) {
		check_slots(document, child);
	}
}

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

Uint64 next_scope_id() {
	static Uint64 s_next = 0;
	return ++s_next;
}

bool is_built_in_attribute(std::string_view name) {
	return name == "id" || name == "class" || name == "font" || name == "style" || name == "src" || name == "uv" ||
		name == "tint";
}

} // namespace

struct LayoutLoader::BuildScope {
	const Markup::MarkupDocument *document = nullptr;
	const Markup::ComponentDefinition *definition = nullptr;
	const Markup::MarkupNode *instance = nullptr;
	const BuildScope *outer = nullptr;
	Uint64 scope_id = 0;
	std::map<std::string, std::string, std::less<>> props{};
	Ref<const Markup::MarkupDocument> owner{};
	std::vector<Unique<View>> *slot_children = nullptr;

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

struct LayoutLoader::BuildPass {
	explicit BuildPass(LayoutLoader &loader) : m_loader(loader) { ++m_loader.m_build_depth; }
	~BuildPass() {
		if (--m_loader.m_build_depth == 0) {
			m_loader.m_visible_imports.clear();
		}
	}
	AQUILA_NONCOPYABLE(BuildPass);
	AQUILA_NONMOVEABLE(BuildPass);
	LayoutLoader &m_loader;
};

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
	return load_root(path, next_scope_id(), nullptr);
}

Unique<View> LayoutLoader::load_root(const std::string &path, Uint64 scope_id,
									 Ref<const Markup::MarkupDocument> *document_out) {
	const BuildPass pass(*this);
	const std::string resolved = resolve_layout_path(m_current_dir, path);

	if (std::ranges::find(m_include_stack, resolved) != m_include_stack.end()) {
		AQUILA_LOG_ERROR("LayoutLoader: include cycle detected at '{}'", resolved);
		return nullptr;
	}

	const Ref<const Markup::MarkupDocument> document = load_document(resolved);
	if (!document || !document->root) {
		return nullptr;
	}
	if (document_out != nullptr) {
		*document_out = document;
	}

	const std::string prev_dir = m_current_dir;
	m_current_dir = Markup::markup_directory(resolved);
	m_include_stack.push_back(resolved);

	const BuildScope scope{ .document = document.get(), .scope_id = scope_id, .owner = document };
	Unique<View> result = build_element(*document->root, scope);

	m_include_stack.pop_back();
	m_current_dir = prev_dir;
	return result;
}

View *LayoutLoader::build_file(const std::string &path, View &parent, Unique<Behaviour> behaviour) {
	const Uint64 scope_id = next_scope_id();
	Ref<const Markup::MarkupDocument> document;
	Unique<View> root = load_root(path, scope_id, &document);
	if (!root) {
		return nullptr;
	}
	View *placed = parent.add_child(std::move(root));
	if (behaviour) {
		auto instance = std::make_unique<ComponentInstance>();
		instance->m_scope_id = scope_id;
		instance->m_name = std::filesystem::path(document->source_name).filename().string();
		instance->m_file = document->source_name;
		instance->m_document = document;
		attach_behaviour(*placed, std::move(instance), std::move(behaviour));
	}
	return placed;
}

View *LayoutLoader::create(const std::string &file, std::string_view component, View &parent, ComponentArgs args) {
	Unique<View> created = create_detached(file, component, std::move(args));
	return created ? parent.add_child(std::move(created)) : nullptr;
}

Unique<View> LayoutLoader::create_detached(const std::string &file, std::string_view component, ComponentArgs args) {
	const BuildPass pass(*this);
	const std::string resolved = Markup::resolve_markup_path(UI_DIR, file, ".aqlayout");
	const Ref<const Markup::MarkupDocument> document = load_document(resolved);
	if (!document) {
		return nullptr;
	}
	const Markup::ComponentDefinition *definition = document->find_component(component);
	if (definition == nullptr) {
		AQUILA_LOG_ERROR("LayoutLoader: no component '{}' in {}", component, resolved);
		return nullptr;
	}
	const BuildScope scope{ .document = document.get(), .owner = document };
	return build_instance({ .definition = definition, .document = document.get(), .owner = document }, scope, component,
						  std::move(args));
}

Unique<View> LayoutLoader::create_detached_from(const ComponentInstance &instance, std::string_view component,
												ComponentArgs args) {
	if (!instance.m_document) {
		return nullptr;
	}
	const BuildPass pass(*this);
	const BuildScope scope{ .document = instance.m_document.get(),
							.scope_id = instance.m_scope_id,
							.owner = instance.m_document };
	const FoundComponent found = find_visible(scope, component);
	if (found.definition == nullptr) {
		AQUILA_LOG_ERROR("LayoutLoader: '{}' can't create '{}', {} doesn't define or import it", instance.m_name,
						 component, instance.m_file);
		return nullptr;
	}
	return build_instance(found, scope, component, std::move(args));
}

Unique<View> LayoutLoader::build_instance(const FoundComponent &found, const BuildScope &scope,
										  std::string_view component, ComponentArgs args) {
	Markup::MarkupNode instance;
	instance.tag = std::string(component);
	for (auto &[name, value] : args.attributes) {
		instance.attributes.push_back(
			{ .name = std::move(name), .value = std::move(value), .where = {}, .value_where = {} });
	}
	return build_component(found, instance, scope, &args.children);
}

void LayoutLoader::report_missing_behaviour(std::string_view file, std::string_view component) const {
	AQUILA_LOG_ERROR("LayoutLoader: '{}' in {} doesn't have the behaviour that was asked for", component, file);
}

void LayoutLoader::attach_behaviour(View &root, Unique<ComponentInstance> instance, Unique<Behaviour> behaviour) {
	instance->m_root = &root;
	instance->m_loader = this;
	instance->m_inner = root.take_component_instance();
	Behaviour *attached = behaviour.get();
	attached->m_instance = instance.get();
	instance->m_behaviour = std::move(behaviour);
	root.set_component_instance(std::move(instance));
	attached->define_behaviour();
}

Ref<const Markup::MarkupDocument> LayoutLoader::load_document(const std::string &path) {
	auto *vfs = Platform::Filesystem::VirtualFileSystem::get();
	const Uint64 write_time = vfs->get_last_write_time(path);
	const Int64 size = vfs->get_file_size(path);
	if (const auto it = m_documents.find(path);
		it != m_documents.end() && it->second.write_time == write_time && it->second.size == size) {
		return it->second.document;
	}

	const std::string src = vfs->read_text_file(path);
	if (src.empty()) {
		AQUILA_LOG_ERROR("LayoutLoader: cannot open '{}'", path);
		m_documents.erase(path);
		return nullptr;
	}

	auto document = std::make_shared<const Markup::MarkupDocument>(Markup::parse_markup(src, path));
	m_documents[path] = { .document = document, .write_time = write_time, .size = size };
	std::vector<std::string> imports;
	for (const Markup::MarkupImport &import : document->imports) {
		imports.push_back(Markup::resolve_markup_path(Markup::markup_directory(path), import.src, ".aqlayout"));
	}
	m_styles.note_layout(path, document->root.has_value(), std::move(imports));
	check_document(*document);
	return document;
}

void LayoutLoader::check_document(const Markup::MarkupDocument &document) {
	std::map<std::string, std::string, std::less<>> defined_in;
	for (const Markup::ComponentDefinition &definition : document.components) {
		if (m_factories.contains(definition.name)) {
			AQUILA_LOG_ERROR("LayoutLoader: {}: <Component '{}'> has the name of a widget",
							 document.location(definition.where), definition.name);
		}
		defined_in.emplace(definition.name, document.source_name);
		check_slots(document, definition.root);
	}

	const std::string directory = directory_of(document);
	m_checking.push_back(document.source_name);
	for (const Markup::MarkupImport &import : document.imports) {
		const std::string path = Markup::resolve_markup_path(directory, import.src, ".aqlayout");
		const std::string where = document.location(import.where);
		if (std::ranges::find(m_checking, path) != m_checking.end()) {
			AQUILA_LOG_ERROR("LayoutLoader: {}: import cycle at '{}'", where, path);
			continue;
		}
		if (!Platform::Filesystem::VirtualFileSystem::get()->exists(path)) {
			AQUILA_LOG_ERROR("LayoutLoader: {}: cannot open '{}'", where, path);
			continue;
		}
		const Ref<const Markup::MarkupDocument> imported = load_document(path);
		if (!imported) {
			continue;
		}
		for (const Markup::ComponentDefinition &definition : imported->components) {
			const auto [it, inserted] = defined_in.emplace(definition.name, path);
			if (!inserted) {
				AQUILA_LOG_ERROR("LayoutLoader: {}: '{}' is defined in both {} and {}", where, definition.name,
								 it->second, path);
			}
		}
	}
	m_checking.pop_back();
}

std::string LayoutLoader::directory_of(const Markup::MarkupDocument &document) const {
	return document.source_name.starts_with('/') ? Markup::markup_directory(document.source_name) : m_current_dir;
}

LayoutLoader::FoundComponent LayoutLoader::find_visible(const BuildScope &scope, std::string_view name) {
	if (const Markup::ComponentDefinition *own = scope.document->find_component(name)) {
		return { .definition = own, .document = scope.document, .owner = scope.owner };
	}
	auto [cached, inserted] = m_visible_imports.try_emplace(scope.document);
	if (inserted) {
		cached->second.importer = scope.owner;
		const std::string directory = directory_of(*scope.document);
		for (const Markup::MarkupImport &import : scope.document->imports) {
			const std::string path = Markup::resolve_markup_path(directory, import.src, ".aqlayout");
			if (!Platform::Filesystem::VirtualFileSystem::get()->exists(path)) {
				continue;
			}
			if (Ref<const Markup::MarkupDocument> imported = load_document(path)) {
				cached->second.imports.push_back(std::move(imported));
			}
		}
	}
	for (const Ref<const Markup::MarkupDocument> &imported : cached->second.imports) {
		if (const Markup::ComponentDefinition *definition = imported->find_component(name)) {
			return { .definition = definition, .document = imported.get(), .owner = imported };
		}
	}
	return {};
}

Unique<View> LayoutLoader::load_string(std::string_view xml) {
	const BuildPass pass(*this);
	const auto document = std::make_shared<const Markup::MarkupDocument>(Markup::parse_markup(xml, "<string>"));
	check_document(*document);
	if (!document->root) {
		return nullptr;
	}
	const BuildScope scope{ .document = document.get(), .scope_id = next_scope_id(), .owner = document };
	return build_element(*document->root, scope);
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
		if (path.starts_with('/')) {
			return load_file(path);
		}
		const std::string directory = directory_of(document);
		return load_file(directory.empty() ? path : directory + "/" + path);
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
		if (const FoundComponent found = find_visible(scope, node.tag); found.definition != nullptr) {
			return build_component(found, node, scope);
		}
		AQUILA_LOG_ERROR("LayoutLoader: {}: unknown widget type '{}'", document.location(node.where), node.tag);
		return nullptr;
	}

	UI::StyleProperties props;
	for (const Markup::MarkupAttribute &attribute : node.attributes) {
		if (attribute.name == "id") {
			view->set_id(scope.substitute(attribute.value, attribute.value_where));
			view->set_id_scope(scope.scope_id);
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

Unique<View> LayoutLoader::build_component(const FoundComponent &found, const Markup::MarkupNode &instance,
										   const BuildScope &scope, std::vector<Unique<View>> *slot_children) {
	const Markup::ComponentDefinition &definition = *found.definition;
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
	BuildScope inner{ .document = found.document,
					  .definition = &definition,
					  .instance = &instance,
					  .outer = &scope,
					  .scope_id = next_scope_id(),
					  .owner = found.owner,
					  .slot_children = slot_children };
	const bool text_is_prop = inner.takes_prop("text");
	const bool has_behaviour = !definition.behaviour.empty();
	const bool text_is_read = text_is_prop || has_behaviour;
	const bool has_children = std::ranges::any_of(instance.children,
												  [text_is_read](const Markup::MarkupNode &child) {
													  return !text_is_read || !child.is_text();
												  }) ||
		(slot_children != nullptr && !slot_children->empty());
	if (!definition.has_slot && has_children) {
		AQUILA_LOG_WARNING("LayoutLoader: {}: '{}' has no <Slot/>, its children are dropped", where, definition.name);
	}

	std::map<std::string, std::string, std::less<>> attributes;
	for (const Markup::MarkupAttribute &attribute : instance.attributes) {
		if (attribute.name == "id" || attribute.name == "class" || attribute.name == "style") {
			continue;
		}
		std::string value = scope.substitute(attribute.value, attribute.value_where);
		if (inner.takes_prop(attribute.name)) {
			inner.props[attribute.name] = value;
		} else if (!has_behaviour) {
			AQUILA_LOG_WARNING("LayoutLoader: {}: '{}' doesn't take the attribute '{}'",
							   document.location(attribute.where), definition.name, attribute.name);
		}
		attributes[attribute.name] = std::move(value);
	}
	std::string text;
	bool has_text = false;
	for (const Markup::MarkupNode &child : instance.children) {
		if (child.is_text()) {
			text += (text.empty() ? "" : " ") + scope.substitute(child.text, child.where);
			has_text = true;
		}
	}
	if (text_is_prop && has_text) {
		inner.props["text"] = text;
	}
	if (!text.empty()) {
		attributes["text"] = std::move(text);
	}

	Unique<View> root = build_element(definition.root, inner);
	if (!root) {
		return nullptr;
	}

	UI::StyleProperties props;
	for (const Markup::MarkupAttribute &attribute : instance.attributes) {
		if (attribute.name == "id") {
			root->set_id(scope.substitute(attribute.value, attribute.value_where));
			root->set_id_scope(scope.scope_id);
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

	if (has_behaviour) {
		Unique<Behaviour> behaviour = m_behaviours.create(definition.behaviour);
		if (!behaviour) {
			AQUILA_LOG_ERROR("LayoutLoader: {}: no behaviour named '{}'", found.document->location(definition.where),
							 definition.behaviour);
			return root;
		}
		auto component = std::make_unique<ComponentInstance>();
		component->m_scope_id = inner.scope_id;
		component->m_name = definition.name;
		component->m_file = found.document->source_name;
		component->m_document = found.owner;
		component->m_attributes = std::move(attributes);
		attach_behaviour(*root, std::move(component), std::move(behaviour));
	}
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
			const Usize first = view.get_children().size();
			if (scope.instance != nullptr && scope.outer != nullptr) {
				build_children(view, *scope.instance, *scope.outer, font, scope.takes_prop("text"));
			}
			if (scope.slot_children != nullptr) {
				for (Unique<View> &placed : *scope.slot_children) {
					view.add_child(std::move(placed));
				}
				scope.slot_children->clear();
			}
			if (const Markup::MarkupAttribute *classes = child.find("class")) {
				std::istringstream iss(scope.substitute(classes->value, classes->value_where));
				std::string cls;
				while (iss >> cls) {
					for (Usize i = first; i < view.get_children().size(); ++i) {
						view.get_children()[i]->add_class(cls);
					}
				}
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
	Register<Clickable>("Clickable");
	Register<SelectableTextView>("SelectableTextView");
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

	m_builtin_behaviours.push_back(m_behaviours.add<InspectorPage>("InspectorPage"));
	m_builtin_behaviours.push_back(m_behaviours.add<OverlayCheckRow>("OverlayCheckRow"));
}

LayoutLoader *find_layout_loader(const View &view) {
	const Canvas *canvas = view.get_canvas();
	return canvas != nullptr ? canvas->get_layout_loader() : nullptr;
}

}
