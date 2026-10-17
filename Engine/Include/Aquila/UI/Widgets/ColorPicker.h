#pragma once

#include "Aquila/UI/Widgets/Control.h"
#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/Popup.h"
#include "Aquila/UI/Widgets/Slider.h"
#include "Aquila/UI/Widgets/TextInput.h"
#include "Aquila/GFX/GfxContext.h"
#include "Aquila/GFX/GfxTexture.h"

namespace Aquila::UI::Core {

/// A swatch that opens an RGB / HSV / hex picker. Needs the GfxContext for its gradient textures.
class ColorPicker : public Control {
  public:
	class PickerArea;

	ColorPicker(GFX::GfxContext &ctx, Vec4 color = Vec4(1.F));

	[[nodiscard]] std::string_view get_type_name() const override { return "ColorPicker"; }

	void set_color(Vec4 color);
	[[nodiscard]] Vec4 get_color() const { return m_color; }

	void set_value(Vec4 color) { set_color(color); }
	[[nodiscard]] Vec4 get_value() const { return m_color; }

	Signal<void(Vec4)> on_changed;

	void on_mouse_press(Platform::MouseButton btn, Vec2 pos) override;
	void on_draw_self(Rendering::DrawList &draw_list) override;

  private:
	enum class Mode { RGB, HSV, HEX };

	struct ChannelRow {
		View *row = nullptr;
		Label *label = nullptr;
		Slider *slider = nullptr;
		TextInput *input = nullptr;
	};

	void init();
	void toggle_popup();
	void set_mode(Mode mode);

	void sync_all();
	void sync_channel_displays();
	void sync_hex_display();
	void rebuild_sv_texture();
	void rebuild_alpha_texture();
	void rebuild_channel_textures();
	void apply_channel_value(int idx, float raw_value);

	[[nodiscard]] static std::string fmt_int(int v);
	[[nodiscard]] static std::string fmt_hex(Vec4 color);
	[[nodiscard]] static bool parse_hex(const std::string &s, Vec4 &out);

	GFX::GfxContext &m_ctx;

	Vec4 m_color = Vec4(1.F);
	float m_h = 0.F, m_s = 0.F, m_v = 1.F;
	Mode m_mode = Mode::RGB;

	Ref<GFX::GfxTexture> m_sv_tex;
	Ref<GFX::GfxTexture> m_hue_tex;
	Ref<GFX::GfxTexture> m_alpha_tex;
	Ref<GFX::GfxTexture> m_ch_tex[4];

	View *m_swatch = nullptr;
	Popup *m_popup = nullptr;
	View *m_preview = nullptr;

	PickerArea *m_sv_area = nullptr;
	PickerArea *m_hue_bar = nullptr;
	PickerArea *m_alpha_bar = nullptr;

	Button *m_rgb_btn = nullptr;
	Button *m_hsv_btn = nullptr;
	Button *m_hex_btn = nullptr;

	ChannelRow m_ch[4];

	View *m_hex_row = nullptr;
	TextInput *m_hex_input = nullptr;

	static constexpr float K_SWATCH_H = 28.F;
	static constexpr float K_POPUP_W = 272.F;
	static constexpr float K_PAD = 8.F;
	static constexpr float K_SVH = 200.F;
	static constexpr float K_BAR_H = 14.F;
	static constexpr Uint32 K_TEX_W = 256;
	static constexpr Uint32 K_SV_TEX_H = 256;
	static constexpr Uint32 K_BAR_TEX_H = 8;
};

} // namespace Aquila::UI::Core
