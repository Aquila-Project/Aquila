#include "Aquila/UI/Markup/MarkupPath.h"

namespace Aquila::UI::Markup {

std::string resolve_markup_path(std::string_view base_dir, std::string_view src, std::string_view extension) {
	const std::string joined = src.starts_with('/') ? std::string(src) : std::string(base_dir) + "/" + std::string(src);

	std::vector<std::string_view> segments;
	std::string_view rest = joined;
	while (!rest.empty()) {
		const Usize slash = rest.find('/');
		const std::string_view segment = rest.substr(0, slash);
		rest = slash == std::string_view::npos ? std::string_view() : rest.substr(slash + 1);
		if (segment.empty() || segment == ".") {
			continue;
		}
		if (segment == "..") {
			if (!segments.empty()) {
				segments.pop_back();
			}
			continue;
		}
		segments.push_back(segment);
	}

	std::string resolved;
	for (const std::string_view segment : segments) {
		resolved += '/';
		resolved += segment;
	}
	if (resolved.empty()) {
		return "/";
	}
	if (segments.back().find('.') == std::string_view::npos) {
		resolved += extension;
	}
	return resolved;
}

std::string markup_directory(std::string_view path) {
	const Usize slash = path.find_last_of('/');
	return slash == std::string_view::npos ? std::string() : std::string(path.substr(0, slash));
}

}
