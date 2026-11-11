#ifndef AQUILA_UI_CORE_MARKUP_ITEM_H
#define AQUILA_UI_CORE_MARKUP_ITEM_H

#include "Aquila/UI/Core/View.h"

namespace Aquila::GFX {
class GfxTexture;
}

namespace Aquila::UI::Core {

class IResourceResolver;

struct MarkupItem {
	MarkupItem() = default;
	MarkupItem(const MarkupItem &) = delete;
	MarkupItem &operator=(const MarkupItem &) = delete;
	MarkupItem(MarkupItem &&) = default;
	MarkupItem &operator=(MarkupItem &&) = default;
	~MarkupItem() = default;

	std::string tag{};
	std::map<std::string, std::string, std::less<>> attributes{};
	std::vector<MarkupItem> items{};
	std::vector<Unique<View>> views{};
	std::string location{};
	Uint64 id_scope = 0;

	[[nodiscard]] std::string_view get(std::string_view name) const;
	[[nodiscard]] GFX::GfxTexture *get_texture(std::string_view name, IResourceResolver *resolver) const;
};

[[nodiscard]] Option<F32> parse_markup_number(std::string_view text);
[[nodiscard]] Option<bool> parse_markup_bool(std::string_view text);

}

#endif
