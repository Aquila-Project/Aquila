#include "UI/Components/ContentTile.h"

#include "Aquila/GFX/GfxTexture.h"
#include "Aquila/UI/Widgets/Clickable.h"
#include "Aquila/UI/Widgets/Image.h"
#include "Aquila/UI/Widgets/Label.h"
#include "UI/Widgets/RenameField.h"

namespace Editor {

using namespace Aquila::UI::Core;
using Aquila::UI::StyleLength;
using Aquila::UI::StyleProperties;

namespace {

constexpr Usize K_LINE_LENGTH = 12;
constexpr F32 K_TILE_PADDING = 40.F;
constexpr F32 K_ICON_SCALE = 0.75F;

StyleProperties fit_thumbnail(const Aquila::GFX::GfxTexture &texture, F32 size) {
	const F32 width = static_cast<F32>(std::max(1U, texture.get_width()));
	const F32 height = static_cast<F32>(std::max(1U, texture.get_height()));
	const F32 scale = size / std::max(width, height);
	StyleProperties style;
	style.width = StyleLength::pixel(width * scale);
	style.height = StyleLength::pixel(height * scale);
	return style;
}

bool is_break(std::string_view name, Usize i) {
	const char c = name[i];
	return c == '.' || c == '_' || c == '-' || c == ' ' ||
		(std::isupper(static_cast<unsigned char>(c)) != 0 &&
		 std::islower(static_cast<unsigned char>(name[i - 1])) != 0);
}

std::string wrap_name(std::string_view name) {
	std::string wrapped;
	while (name.size() > K_LINE_LENGTH) {
		Usize cut = K_LINE_LENGTH;
		for (Usize i = K_LINE_LENGTH; i > 0; --i) {
			if (is_break(name, i)) {
				cut = i;
				break;
			}
		}
		wrapped.append(name.substr(0, cut)).push_back('\n');
		name.remove_prefix(cut);
		if (name.starts_with(' ')) {
			name.remove_prefix(1);
		}
	}
	return wrapped.append(name);
}

std::string to_upper(std::string_view text) {
	std::string result(text);
	std::ranges::transform(result, result.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
	return result;
}
}

void ContentTile::define_behaviour() {
	m_clickable = dynamic_cast<Clickable *>(&root());
	m_preview = get<View>("preview");
	m_image = get<Image>("image");
	m_badge = get<View>("badge");
	m_badge_text = get<Label>("badge-text");
	m_name = get<Label>("name");
	if (m_clickable != nullptr) {
		listen(m_clickable->on_drag_start_requested, [this](DragState &state) {
			state.payload = m_payload;
			state.label = m_payload.display_name;
		});
	}
}

void ContentTile::show(AssetPayload asset, bool is_directory, Aquila::GFX::GfxTexture *icon, bool thumbnail,
					   const Config::ContentBrowserPreferences &prefs) {
	m_payload = std::move(asset);
	m_size = prefs.thumbnail_size;
	root().set_tooltip(m_payload.display_name);
	if (m_clickable != nullptr) {
		m_clickable->set_draggable(!is_directory);
	}

	StyleProperties tile_style;
	tile_style.width = StyleLength::pixel(m_size + K_TILE_PADDING);
	root().merge_style(tile_style);
	if (m_preview != nullptr) {
		StyleProperties preview_style;
		preview_style.width = StyleLength::pixel(m_size);
		preview_style.height = StyleLength::pixel(m_size);
		m_preview->merge_style(preview_style);
	}
	if (m_image != nullptr) {
		m_image->set_texture(icon);
		if (thumbnail) {
			set_thumbnail(icon);
		} else {
			m_image->set_class("content-tile-folder", is_directory);
			StyleProperties icon_style;
			icon_style.width = StyleLength::pixel(std::round(m_size * K_ICON_SCALE));
			icon_style.height = StyleLength::pixel(std::round(m_size * K_ICON_SCALE));
			m_image->set_style(icon_style);
		}
	}
	const std::string_view name = m_payload.display_name;
	const Usize dot = name.find_last_of('.');
	const bool has_extension = !is_directory && dot != std::string_view::npos && dot > 0 && dot + 1 < name.size();
	if (m_badge != nullptr && m_badge_text != nullptr) {
		m_badge->set_hidden(!has_extension);
		if (has_extension) {
			m_badge_text->set_text(to_upper(name.substr(dot + 1)));
			m_badge->set_class("content-tile-badge-" + m_payload.asset_type, true);
		}
	}
	if (m_name != nullptr) {
		const bool strip = has_extension && !prefs.show_extensions;
		m_name->set_text(wrap_name(strip ? name.substr(0, dot) : name));
	}
}

void ContentTile::set_thumbnail(Aquila::GFX::GfxTexture *texture) {
	if (texture == nullptr || m_image == nullptr) {
		return;
	}
	m_image->set_texture(texture);
	m_image->set_class("content-tile-icon", false);
	m_image->set_class("content-tile-thumbnail", true);
	m_image->set_style(fit_thumbnail(*texture, m_size));
}

void ContentTile::start_rename(Delegate<void(Option<std::string>)> done) {
	auto *field = dynamic_cast<RenameField *>(create("ContentRename", root()));
	if (field == nullptr) {
		return;
	}
	root().set_input_leaf(false);
	if (m_name != nullptr) {
		m_name->set_hidden(true);
	}
	field->set_on_done(std::move(done));
	field->set_text(m_payload.display_name);
	field->request_focus();
	field->select_stem();
}

}
