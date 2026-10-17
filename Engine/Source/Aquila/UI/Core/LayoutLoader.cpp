#include "Aquila/Foundation/Macros.h"
#include "Aquila/UI/Core/LayoutLoader.h"
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

struct Parser {
	std::string_view src;
	size_t pos = 0;
	LayoutLoader &loader;

	[[nodiscard]] bool at_end() const { return pos >= src.size(); }
	[[nodiscard]] char peek() const { return at_end() ? '\0' : src[pos]; }
	[[nodiscard]] char peek2() const { return (pos + 1 < src.size()) ? src[pos + 1] : '\0'; }

	char advance() {
		AQUILA_ASSERT(!at_end(), "LayoutLoader: unexpected end of input");
		return src[pos++];
	}

	void skip_ws() {
		while (!at_end() && (std::isspace(static_cast<unsigned char>(peek())) != 0)) {
			advance();
		}
	}

	bool match(char c) {
		if (peek() == c) {
			advance();
			return true;
		}
		return false;
	}

	void expect(char c) {
		if (!match(c)) {
			AQUILA_LOG_ERROR("LayoutLoader: expected '{}' at position {}", c, pos);
		}
	}

	std::string read_name() {
		size_t start = pos;
		while (!at_end() &&
			   ((std::isalnum(static_cast<unsigned char>(peek())) != 0) || peek() == '-' || peek() == '_' ||
				peek() == ':')) {
			advance();
		}
		return std::string(src.substr(start, pos - start));
	}

	std::string read_quoted() {
		char q = advance();
		size_t start = pos;
		while (!at_end() && peek() != q) {
			advance();
		}
		std::string val(src.substr(start, pos - start));
		if (!at_end()) {
			advance();
		}
		return val;
	}

	std::string read_unquoted() {
		size_t start = pos;
		while (!at_end() && peek() != '>' && peek() != '/' && (std::isspace(static_cast<unsigned char>(peek())) == 0)) {
			advance();
		}
		return std::string(src.substr(start, pos - start));
	}

	std::string read_text_content() {
		size_t start = pos;
		while (!at_end() && peek() != '<') {
			advance();
		}
		std::string_view raw = src.substr(start, pos - start);
		while (!raw.empty() && (std::isspace(static_cast<unsigned char>(raw.front())) != 0)) {
			raw.remove_prefix(1);
		}
		while (!raw.empty() && (std::isspace(static_cast<unsigned char>(raw.back())) != 0)) {
			raw.remove_suffix(1);
		}
		return std::string(raw);
	}

	bool try_skip_comment() {
		if (pos + 3 < src.size() && src[pos] == '<' && src[pos + 1] == '!' && src[pos + 2] == '-' &&
			src[pos + 3] == '-') {
			pos += 4;
			while (pos + 2 < src.size()) {
				if (src[pos] == '-' && src[pos + 1] == '-' && src[pos + 2] == '>') {
					pos += 3;
					return true;
				}
				++pos;
			}
		}
		return false;
	}

	Unique<View> parse_element() {
		expect('<');
		skip_ws();

		std::string tag_name = read_name();
		if (tag_name.empty()) {
			AQUILA_LOG_ERROR("LayoutLoader: empty tag name at position {}", pos);
			return nullptr;
		}

		std::string id;
		std::vector<std::string> classes;
		UI::StyleProperties props;
		Text::FontAtlas *font = loader.resolve_font("default");

		std::string image_src;
		std::string image_uv;
		std::string image_tint;
		std::vector<std::pair<std::string, std::string>> generic_attrs;

		skip_ws();
		while (!at_end() && peek() != '>' && peek() != '/') {
			std::string attr_name = read_name();
			if (attr_name.empty()) {
				break;
			}

			skip_ws();
			std::string attr_value;
			if (match('=')) {
				skip_ws();
				attr_value = (peek() == '"' || peek() == '\'') ? read_quoted() : read_unquoted();
			}
			skip_ws();

			if (attr_name == "id") {
				id = attr_value;
			} else if (attr_name == "class") {
				std::istringstream iss(attr_value);
				std::string cls;
				while (iss >> cls) {
					classes.push_back(cls);
				}
			} else if (attr_name == "font") {
				font = loader.resolve_font(attr_value);
			} else if (attr_name == "style") {
				std::string_view decls = attr_value;
				while (!decls.empty()) {
					const auto semi = decls.find(';');
					std::string_view decl = (semi != std::string_view::npos) ? decls.substr(0, semi) : decls;
					decls = (semi != std::string_view::npos) ? decls.substr(semi + 1) : std::string_view{};

					const auto colon = decl.find(':');
					if (colon == std::string_view::npos) {
						continue;
					}
					auto trim_sv = [](std::string_view s) {
						while (!s.empty() && static_cast<unsigned char>(s.front()) <= ' ') {
							s.remove_prefix(1);
						}
						while (!s.empty() && static_cast<unsigned char>(s.back()) <= ' ') {
							s.remove_suffix(1);
						}
						return s;
					};
					const std::string_view prop = trim_sv(decl.substr(0, colon));
					const std::string_view val = trim_sv(decl.substr(colon + 1));
					if (!prop.empty() && !val.empty()) {
						UI::StyleParser::apply_property(props, prop, val);
					}
				}
			} else if (attr_name == "src") {
				image_src = attr_value;
			} else if (attr_name == "uv") {
				image_uv = attr_value;
			} else if (attr_name == "tint") {
				image_tint = attr_value;
			} else {
				generic_attrs.emplace_back(std::move(attr_name), std::move(attr_value));
			}
		}

		if (tag_name == "Include") {
			if (!at_end() && peek() == '/') {
				advance();
			}
			expect('>');
			if (image_src.empty()) {
				AQUILA_LOG_ERROR("LayoutLoader: <Include> is missing a 'src' attribute");
				return nullptr;
			}
			return loader.load_file(image_src);
		}

		Unique<View> view = loader.create_widget(tag_name, "", font);
		if (!view) {
			AQUILA_LOG_ERROR("LayoutLoader: unknown widget type '{}'", tag_name);

			while (!at_end() && peek() != '>') {
				advance();
			}
			if (!at_end()) {
				advance();
			}
			return nullptr;
		}

		if (!id.empty()) {
			view->set_id(id);
		}
		for (auto &cls : classes) {
			view->add_class(cls);
		}
		view->merge_style(props);

		if (font != nullptr) {
			view->set_font(font);
		}

		IResourceResolver *loader_ctx = &loader;
		if (!image_tint.empty()) {
			view->apply_xml_attribute("tint", image_tint, loader_ctx);
		}
		if (!image_src.empty()) {
			view->apply_xml_attribute("src", image_src, loader_ctx);
		}
		if (!image_uv.empty()) {
			view->apply_xml_attribute("uv", image_uv, loader_ctx);
		}

		for (const auto &attr : generic_attrs) {
			view->apply_xml_attribute(attr.first, attr.second, loader_ctx);
		}

		if (!at_end() && peek() == '/') {
			advance();
			expect('>');
			return view;
		}

		expect('>');

		while (!at_end()) {
			skip_ws();
			if (at_end()) {
				break;
			}

			if (peek() == '<' && peek2() == '/') {
				pos += 2;
				skip_ws();
				read_name();
				skip_ws();
				expect('>');
				break;
			}

			if (peek() == '<' && pos + 1 < src.size() && src[pos + 1] == '!') {
				try_skip_comment();
				continue;
			}

			if (peek() == '<') {
				auto child = parse_element();
				if (child) {
					view->add_child(std::move(child));
				}
				continue;
			}

			std::string text = read_text_content();
			if (!text.empty()) {
				view->apply_xml_text_content(text);

				if (font != nullptr) {
					view->set_font(font);
				}
			}
		}

		view->on_xml_loaded();
		return view;
	}

	Unique<View> parse() {
		skip_ws();
		while (!at_end()) {
			if (peek() == '<' && peek2() == '!') {
				try_skip_comment();
				skip_ws();
				continue;
			}
			if (peek() == '<' && pos + 1 < src.size() && src[pos + 1] == '?') {
				while (!at_end() && peek() != '>') {
					advance();
				}
				if (!at_end()) {
					advance();
				}
				skip_ws();
				continue;
			}
			if (peek() == '<') {
				return parse_element();
			}
			advance();
		}
		return nullptr;
	}
};

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

void LayoutLoader::register_command(const std::string &name, Delegate<void()> command) {
	m_commands[name] = std::move(command);
}

Delegate<void()> LayoutLoader::resolve_command(const std::string &name) const {
	auto it = m_commands.find(name);
	return (it != m_commands.end()) ? it->second : Delegate<void()>{};
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

	Unique<View> result = LoadString(src);

	m_include_stack.pop_back();
	m_current_dir = prev_dir;
	return result;
}

Unique<View> LayoutLoader::LoadString(std::string_view xml) {
	Parser p{ .src = xml, .pos = 0, .loader = *this };
	return p.parse();
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
