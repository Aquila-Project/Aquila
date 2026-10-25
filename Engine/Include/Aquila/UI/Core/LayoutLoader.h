#pragma once

#include "Aquila/Foundation/Macros.h"
#include "Aquila/UI/Core/Shortcuts.h"
#include "Aquila/UI/Core/View.h"
#include "Aquila/UI/Core/IResourceResolver.h"
#include "Aquila/UI/Core/TextureCache.h"
#include "Aquila/UI/Text/FontAtlas.h"

namespace Aquila::UI::Markup {
struct ComponentDefinition;
struct MarkupDocument;
struct MarkupNode;
}

namespace Aquila::UI::Core {

/// Builds a View tree from .aqlayout markup. Problems are logged with file:line:col and never throw.
///
/// @code
/// LayoutLoader loader;
/// loader.set_default_font(font);
/// loader.set_command_source(&commands, &shortcuts);
/// Unique<View> root = loader.load_file("/resources/Engine/UI/editor.aqlayout");
/// @endcode
///
/// Commands are then usable from markup: `<Button on-click="file.save">Save</Button>`.
class LayoutLoader : public IResourceResolver {
  public:
	using WidgetFactory = Delegate<Unique<View>(std::string_view text, Text::FontAtlas *font)>;

	LayoutLoader();

	void register_font(const std::string &name, Text::FontAtlas *font);
	void set_default_font(Text::FontAtlas *font);
	/// Adds a tag. The factory gets the element's text and font.
	void register_widget(const std::string &type_name, WidgetFactory factory);

	// Texture-based image loading for <Image src="..."/>.
	void register_texture_cache(TextureCache *cache);
	[[nodiscard]] GFX::GfxTexture *resolve_texture(const std::string &path) const override;

	/// Add the commands before loading, on-click is resolved while the markup loads.
	void set_command_source(const Commands *commands, const Shortcuts *shortcuts = nullptr);
	[[nodiscard]] Delegate<void()> resolve_command(const std::string &name) const override;
	[[nodiscard]] CommandInfo query_command(std::string_view name) const override;

	/// VFS path. Null when the file can't be read or an <Include> loops.
	Unique<View> load_file(const std::string &path);
	Unique<View> load_string(std::string_view xml);

	[[nodiscard]] Text::FontAtlas *resolve_font(const std::string &name) const;
	[[nodiscard]] Unique<View> create_widget(const std::string &type, std::string_view text,
											 Text::FontAtlas *font) const;

  private:
	std::unordered_map<std::string, Text::FontAtlas *> m_fonts;
	Text::FontAtlas *m_default_font = nullptr;
	std::unordered_map<std::string, WidgetFactory> m_factories;

	TextureCache *m_texture_cache = nullptr;
	std::unordered_map<std::string, Delegate<void()>> m_commands;

	std::string m_current_dir;
	std::vector<std::string> m_include_stack;

	const Commands *m_command_source = nullptr;
	const Shortcuts *m_shortcut_source = nullptr;

	template <typename T> void Register(const std::string &type_name) {
		m_factories[type_name] = [](std::string_view, Text::FontAtlas *) -> Unique<View> {
			return std::make_unique<T>();
		};
	}

	void register_builtins();
	struct BuildScope;

	struct CachedDocument {
		Ref<const Markup::MarkupDocument> document;
		Uint64 write_time = 0;
		Int64 size = -1;
	};

	struct FoundComponent {
		const Markup::ComponentDefinition *definition = nullptr;
		const Markup::MarkupDocument *document = nullptr;
		Ref<const Markup::MarkupDocument> owner;
	};

	std::unordered_map<std::string, CachedDocument> m_documents;
	std::vector<std::string> m_checking;

	[[nodiscard]] Ref<const Markup::MarkupDocument> load_document(const std::string &path);
	void check_document(const Markup::MarkupDocument &document);
	[[nodiscard]] std::string directory_of(const Markup::MarkupDocument &document) const;
	[[nodiscard]] FoundComponent find_visible(const BuildScope &scope, std::string_view name);

	Unique<View> build_element(const Markup::MarkupNode &node, const BuildScope &scope);
	Unique<View> build_component(const FoundComponent &found, const Markup::MarkupNode &instance,
								 const BuildScope &scope);
	void build_children(View &view, const Markup::MarkupNode &node, const BuildScope &scope, Text::FontAtlas *font,
						bool skip_text = false);
};

} // namespace Aquila::UI::Core
