#pragma once

#include "Aquila/UI/Core/View.h"

namespace Aquila::GFX {
class GfxTexture;
}

namespace Aquila::UI::Core {

/// Anything a panel should own and destroy with itself.
class DockPanelContent {
  public:
	virtual ~DockPanelContent() = default;
};

class DockPanel : public View {
  public:
	explicit DockPanel(std::string title = "");
	~DockPanel() override;

	[[nodiscard]] std::string_view get_type_name() const override { return "DockPanel"; }
	static constexpr ViewKind k_kind = ViewKind::DockPanel;
	[[nodiscard]] ViewKind get_kind() const override { return k_kind; }
	[[nodiscard]] const std::string &get_title() const { return m_title; }
	void set_title(std::string title);

	[[nodiscard]] GFX::GfxTexture *get_tab_icon() const { return m_tab_icon; }
	void set_tab_icon(GFX::GfxTexture *icon) { m_tab_icon = icon; }

	/// The panel destroys it with itself, so closing a tab, switching workspace or tearing off cleans up.
	void set_content_owner(Unique<DockPanelContent> owner) { m_content_owner = std::move(owner); }
	[[nodiscard]] DockPanelContent *get_content_owner() const { return m_content_owner.get(); }

	void apply_xml_text_content(std::string_view text) override { set_title(std::string(text)); }
	void apply_xml_attribute(std::string_view name, std::string_view value, IResourceResolver *resolver = nullptr) override;

  private:
	std::string m_title;
	GFX::GfxTexture *m_tab_icon = nullptr;
	Unique<DockPanelContent> m_content_owner;
};

} // namespace Aquila::UI::Core
