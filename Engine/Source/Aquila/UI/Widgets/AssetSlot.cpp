#include "Aquila/UI/Widgets/AssetSlot.h"

namespace Aquila::UI::Core {

AssetSlot::AssetSlot() {
	m_is_accepting_payload = true;
	add_class("asset-slot");

	auto label = std::make_unique<Label>("");
	label->add_class("asset-slot-label");
	m_label = dynamic_cast<Label *>(add_child(std::move(label)));

	auto clear_btn = std::make_unique<Button>("×");
	clear_btn->add_class("asset-slot-clear");
	clear_btn->on_click.connect([this] { clear(); });
	m_clear_button = dynamic_cast<Button *>(add_child(std::move(clear_btn)));

	update_display();
}

AssetSlot::AssetSlot(std::string accepted_type) : AssetSlot() {
	m_accepted_type = std::move(accepted_type);
	update_display();
}

void AssetSlot::set_accepted_type(std::string type) {
	m_accepted_type = std::move(type);
	update_display();
}

void AssetSlot::set_placeholder(std::string text) {
	m_placeholder = std::move(text);
	update_display();
}

void AssetSlot::set_value(AssetPayload asset) {
	m_value = std::move(asset);
	m_has_value = true;
	update_display();
}

void AssetSlot::clear() {
	m_has_value = false;
	m_value = {};
	update_display();
	on_changed(std::nullopt);
}

void AssetSlot::on_drop(DragState &state) {
	if (!state.payload.has_value()) {
		return;
	}

	try {
		AssetPayload payload = std::any_cast<AssetPayload>(state.payload);
		if (!is_compatible(payload)) {
			remove_class("drag-over-invalid");
			return;
		}
		set_value(payload);
		remove_class("drag-over");
		remove_class("drag-over-invalid");
		on_changed(m_value);
	} catch (const std::bad_any_cast &) {
	}
}

void AssetSlot::on_drag_enter(DragState &state) {
	if (!state.payload.has_value()) {
		return;
	}
	const auto *payload = std::any_cast<AssetPayload>(&state.payload);
	if (payload != nullptr && is_compatible(*payload)) {
		add_class("drag-over");
		show_hint("Drop to use " + payload->display_name);
	} else {
		add_class("drag-over-invalid");
		show_hint("Not " + accepted_name());
	}
}

void AssetSlot::on_drag_leave(DragState &) {
	remove_class("drag-over");
	remove_class("drag-over-invalid");
	update_display();
}

void AssetSlot::apply_xml_attribute(std::string_view name, std::string_view value, IResourceResolver *resolver) {
	if (name == "accept") {
		set_accepted_type(std::string(value));
		return;
	}
	if (name == "placeholder") {
		set_placeholder(std::string(value));
		return;
	}
	View::apply_xml_attribute(name, value, resolver);
}

void AssetSlot::update_display() {
	m_clear_button->set_hidden(!m_has_value);
	set_class("asset-slot-filled", m_has_value);

	m_label->set_class("asset-slot-placeholder", !m_has_value);
	if (m_has_value) {
		m_label->set_text(m_value.display_name.empty() ? m_value.asset_path : m_value.display_name);
	} else {
		m_label->set_text(m_placeholder.empty() ? "Drop " + accepted_name() + " here" : m_placeholder);
	}
}

void AssetSlot::show_hint(std::string text) {
	m_label->set_class("asset-slot-placeholder", false);
	m_label->set_text(std::move(text));
}

std::string AssetSlot::accepted_name() const {
	if (m_accepted_type.empty()) {
		return "an asset";
	}
	const bool vowel = std::string_view("aeiouAEIOU").find(m_accepted_type.front()) != std::string_view::npos;
	return (vowel ? "an " : "a ") + m_accepted_type;
}

bool AssetSlot::is_compatible(const AssetPayload &payload) const {
	if (m_accepted_type.empty()) {
		return true;
	}
	return payload.asset_type == m_accepted_type;
}

} // namespace Aquila::UI::Core
