#ifndef AQUILA_TESTS_UI_STYLE_SNAPSHOT_H
#define AQUILA_TESTS_UI_STYLE_SNAPSHOT_H

#include "Aquila/UI/Core/View.h"

namespace StyleSnapshotDetail {

inline void write_length(std::ostream &out, const char *name, const Aquila::UI::StyleLength &length) {
	out << ' ' << name << '=' << static_cast<int>(length.unit) << ':' << length.value;
}

inline void write_color(std::ostream &out, const char *name, const Vec4 &color) {
	out << ' ' << name << '=' << color.r << ',' << color.g << ',' << color.b << ',' << color.a;
}

inline void write_view(std::ostream &out, const Aquila::UI::Core::View &view, int depth) {
	using namespace Aquila::UI;
	out << std::string(static_cast<Usize>(depth) * 2, ' ') << view.get_type_name();
	std::vector<std::string> classes = view.get_classes();
	std::ranges::sort(classes);
	for (const std::string &name : classes) {
		out << '.' << name;
	}
	const Rect &rect = view.get_layout_rect();
	out << " rect=" << rect.position.x << ',' << rect.position.y << ',' << rect.size.x << ',' << rect.size.y;

	const ComputedStyle &style = view.get_computed_style();
	write_color(out, "background", style.background_color);
	write_color(out, "border", style.border_color);
	write_color(out, "radius", style.border_radius);
	out << " border-width=" << style.border_width << " border-style=" << static_cast<int>(style.border_style)
		<< " cursor=" << static_cast<int>(style.cursor) << " opacity=" << style.opacity;
	write_length(out, "width", style.width);
	write_length(out, "height", style.height);
	write_length(out, "min-width", style.min_width);
	write_length(out, "max-width", style.max_width);
	write_length(out, "min-height", style.min_height);
	write_length(out, "max-height", style.max_height);
	write_length(out, "padding-top", style.padding.top);
	write_length(out, "padding-right", style.padding.right);
	write_length(out, "padding-bottom", style.padding.bottom);
	write_length(out, "padding-left", style.padding.left);
	out << " gap=" << style.gap << " aspect=" << style.aspect_ratio
		<< " direction=" << static_cast<int>(style.flex_direction) << " justify=" << static_cast<int>(style.justify)
		<< " align=" << static_cast<int>(style.align) << " wrap=" << static_cast<int>(style.wrap)
		<< " grow=" << style.flex_grow << " position=" << static_cast<int>(style.position);
	write_length(out, "top", style.top);
	write_length(out, "bottom", style.bottom);
	write_length(out, "left", style.left);
	write_length(out, "right", style.right);
	out << " z=" << style.z_index << " display=" << static_cast<int>(style.display)
		<< " overflow=" << static_cast<int>(style.overflow);
	write_color(out, "color", style.color);
	write_color(out, "accent", style.accent_color);
	write_color(out, "selection", style.selection_color);
	write_color(out, "placeholder", style.placeholder_color);
	out << " font-size=" << style.font_size << " font=" << style.font_family
		<< " text-align=" << static_cast<int>(style.text_align)
		<< " white-space=" << static_cast<int>(style.white_space);
	for (const BoxShadow &shadow : style.box_shadows) {
		out << " shadow=" << shadow.offset.x << ',' << shadow.offset.y << ',' << shadow.blur << ',' << shadow.spread
			<< ',' << shadow.inset;
		write_color(out, "shadow-color", shadow.color);
	}
	out << " transition=" << style.transition_duration << ':' << static_cast<int>(style.transition_easing) << '\n';

	for (const auto &child : view.get_children()) {
		write_view(out, *child, depth + 1);
	}
}

}

inline std::string style_snapshot(const Aquila::UI::Core::View &root) {
	std::ostringstream out;
	out << std::fixed << std::setprecision(3);
	StyleSnapshotDetail::write_view(out, root, 0);
	return out.str();
}

inline void write_style_snapshot(const std::string &name, const Aquila::UI::Core::View &root) {
	std::filesystem::create_directories("StyleSnapshots");
	std::ofstream("StyleSnapshots/" + name + ".txt", std::ios::trunc) << style_snapshot(root);
}

#endif
