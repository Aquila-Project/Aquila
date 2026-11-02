#include "Aquila/UI/Core/MarkupItem.h"

#include "Aquila/UI/Core/IResourceResolver.h"

namespace Aquila::UI::Core {

std::string_view MarkupItem::get(std::string_view name) const {
	const auto it = attributes.find(name);
	return it != attributes.end() ? std::string_view(it->second) : std::string_view();
}

GFX::GfxTexture *MarkupItem::get_texture(std::string_view name, IResourceResolver *resolver) const {
	const std::string_view path = get(name);
	return resolver != nullptr && !path.empty() ? resolver->resolve_texture(std::string(path)) : nullptr;
}

Option<F32> parse_markup_number(std::string_view text) {
	F32 number = 0.F;
	const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), number);
	if (error != std::errc{} || end != text.data() + text.size() || text.empty()) {
		return std::nullopt;
	}
	return number;
}

Option<bool> parse_markup_bool(std::string_view text) {
	if (text == "true") {
		return true;
	}
	if (text == "false") {
		return false;
	}
	return std::nullopt;
}

}
