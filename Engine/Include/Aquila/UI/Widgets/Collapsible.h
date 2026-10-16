#pragma once

#include "Aquila/UI/Core/View.h"
#include "Aquila/UI/Widgets/Button.h"

namespace Aquila::GFX {
class GfxTexture;
}

namespace Aquila::UI::Core {

class Collapsible : public View {
  public:
	explicit Collapsible(std::string title = "");

	[[nodiscard]] std::string_view get_type_name() const override { return "Collapsible"; }
	static constexpr ViewKind k_kind = ViewKind::Collapsible;
	[[nodiscard]] ViewKind get_kind() const override { return k_kind; }

	void set_title(std::string title);
	void set_icon(GFX::GfxTexture *icon);
	void set_variant(const std::string &variant);
	void set_reorderable(bool reorderable);
	[[nodiscard]] bool is_reorderable() const { return m_reorderable; }
	void set_collapsible(bool collapsible) { m_collapsible = collapsible; }
	[[nodiscard]] bool is_collapsible() const { return m_collapsible; }
	void apply_xml_text_content(std::string_view text) override { set_title(std::string(text)); }
	void apply_xml_attribute(std::string_view name, std::string_view value, IResourceResolver *resolver = nullptr) override;
	void set_expanded(bool expanded);
	[[nodiscard]] bool is_expanded() const { return m_expanded; }
	Button *add_header_action(GFX::GfxTexture *icon, std::string tooltip, Delegate<void()> on_click);
	void set_state_icons(GFX::GfxTexture *collapsed, GFX::GfxTexture *expanded, bool leading = false);
	Signal<void(bool)> on_toggled;
	Signal<void()> on_reordered;

	using View::add_child;
	View *add_child(Unique<View> child) override;

	void begin_drag();
	bool on_update(F32 delta_time) override;

  private:
	void toggle();
	void apply_state();
	void seed_flip_homes();
	void update_drag();
	void update_drop_target();
	void finalize_drag();
	void tick_flip(F32 delta_time);
	void start_ticking();
	void stop_ticking();

	bool m_reorderable = true;
	bool m_collapsible = true;
	bool m_expanded = true;
	GFX::GfxTexture *m_icon_collapsed = nullptr;
	GFX::GfxTexture *m_icon_expanded = nullptr;
	bool m_state_icons_leading = false;
	View *m_header_bar = nullptr;
	Button *m_title_button = nullptr;
	Button *m_chevron = nullptr;
	std::string m_variant;
	View *m_grip = nullptr;
	View *m_content = nullptr;

	StyleProperties m_pre_drag_style;
	bool m_dragging = false;
	bool m_ticking = false;
	View *m_container = nullptr;
	View *m_placeholder = nullptr;
	View *m_drop_anchor = nullptr;
	Vec2 m_grab_offset{};
	F32 m_locked_x = 0.0F;
	std::unordered_map<View *, Vec2> m_flip_home;
};

} // namespace Aquila::UI::Core
