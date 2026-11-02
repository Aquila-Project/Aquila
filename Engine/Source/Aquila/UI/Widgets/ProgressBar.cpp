#include "Aquila/UI/Widgets/ProgressBar.h"
#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/UI/Core/MarkupItem.h"

namespace Aquila::UI::Core {

ProgressBar::ProgressBar() {
	add_class("progress-bar");

	auto fill = std::make_unique<View>();
	fill->add_class("progress-fill");
	m_fill = add_child(std::move(fill));

	set_value(0.F);
}

void ProgressBar::set_value(float value) {
	m_value = Math::clamp(value, 0.F, 1.F);

	StyleProperties sp;
	sp.width = StyleLength::percent(m_value * 100.F);
	sp.height = StyleLength::grow();
	m_fill->merge_style(sp);
}

void ProgressBar::apply_xml_attribute(std::string_view name, std::string_view value, IResourceResolver *resolver) {
	if (name == "value") {
		if (const Option<F32> number = parse_markup_number(value)) {
			set_value(*number);
		} else {
			AQUILA_LOG_WARNING("ProgressBar: value expects a number, got '{}'", value);
		}
		return;
	}
	View::apply_xml_attribute(name, value, resolver);
}

} // namespace Aquila::UI::Core
