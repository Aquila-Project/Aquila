#include "Aquila/UI/Widgets/AssetCard.h"

namespace Aquila::UI::Core {

AssetCard::AssetCard() {
	m_is_draggable = true;
	add_class("asset-card");

	auto thumbnail = std::make_unique<Image>();
	thumbnail->add_class("asset-card-thumbnail");
	m_thumbnail = add_child(std::move(thumbnail));

	auto type_label = std::make_unique<Label>("");
	type_label->add_class("asset-card-type");
	m_type_label = add_child(std::move(type_label));

	auto name_label = std::make_unique<Label>("");
	name_label->add_class("asset-card-name");
	m_name_label = add_child(std::move(name_label));
}

void AssetCard::set_asset(AssetPayload payload) {
	m_payload = std::move(payload);

	std::string display_name = m_payload.display_name.empty() ? m_payload.asset_path : m_payload.display_name;
	m_name_label->set_text(display_name);
	m_type_label->set_text(m_payload.asset_type);
}

void AssetCard::set_thumbnail(GFX::GfxTexture *texture) {
	m_thumbnail->set_texture(texture);

	m_thumbnail->set_hidden(texture == nullptr);
}

void AssetCard::on_drag_start(DragState &state) {
	state.payload = m_payload;
}

} // namespace Aquila::UI::Core
