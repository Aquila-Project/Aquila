#include "Aquila/UI/Widgets/ColorPicker.h"
#include "Aquila/Foundation/Color.h"
#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/RHI/Backend/RHITypes.h"

namespace Aquila::UI::Core {

using Aquila::Foundation::Color::gen_alpha;
using Aquila::Foundation::Color::gen_channel_grad;
using Aquila::Foundation::Color::gen_hue;
using Aquila::Foundation::Color::gen_sv;
using Aquila::Foundation::Color::hsv_to_rgb;
using Aquila::Foundation::Color::rgb_to_hsv;

class ColorPicker::PickerArea : public View {
  public:
	GFX::GfxTexture *m_tex = nullptr;
	Vec2 m_indicator = {};
	bool m_is1_d = false;
	Delegate<void(Vec2)> m_on_pick;

	PickerArea() { set_input_leaf(true); }

	void on_mouse_press(Platform::MouseButton btn, Vec2 pos) override {
		View::on_mouse_press(btn, pos);
		if (btn == Platform::MouseButton::Left) {
			pick(pos);
		}
	}
	void on_mouse_move(Vec2 pos) override {
		if (m_is_pressed) {
			pick(pos);
		}
	}

	void on_draw_self(Rendering::DrawList &dl) override {
		View::on_draw_self(dl);
		const Rect r = { get_absolute_position(), get_layout_rect().size };
		const Int32 z = 0;

		if (m_tex != nullptr) {
			dl.draw_image(r, m_tex, Vec4(1.F), Vec2(0.F), Vec2(1.F), z + 1);
		}

		if (m_is1_d) {
			const float ix = r.position.x + m_indicator.x * r.size.x;
			const Rect ind = { .position = { ix - 3.F, r.position.y - 2.F }, .size = { 6.F, r.size.y + 4.F } };
			dl.draw_rect(ind, Vec4(1.F), Vec4(3.F), 1.5F, Vec4(0.1F, 0.1F, 0.1F, 0.9F), z + 2);
		} else {
			const Vec2 ic = r.position + m_indicator * r.size;
			const float rd = 6.F;
			const Rect ind = { .position = { ic.x - rd, ic.y - rd }, .size = { rd * 2.F, rd * 2.F } };
			dl.draw_rect(ind, Vec4(0.F, 0.F, 0.F, 0.F), Vec4(rd), 2.F, Vec4(0.F, 0.F, 0.F, 0.9F), z + 2);
			const float ri = 4.F;
			const Rect i2 = { .position = { ic.x - ri, ic.y - ri }, .size = { ri * 2.F, ri * 2.F } };
			dl.draw_rect(i2, Vec4(0.F), Vec4(ri), 1.5F, Vec4(1.F, 1.F, 1.F, 0.85F), z + 3);
		}
	}

  private:
	void pick(Vec2 abs_pos) {
		const Rect r = get_absolute_rect();
		Vec2 n = (abs_pos - r.position) / Math::max(r.size, Vec2(1.F));
		n = Math::clamp(n, Vec2(0.F), Vec2(1.F));
		if (m_is1_d) {
			n.y = 0.F;
		}
		m_indicator = n;
		queue_redraw();
		if (m_on_pick) {
			m_on_pick(n);
		}
	}
};

static Ref<GFX::GfxTexture> make_tex(GFX::GfxContext &ctx, Uint32 w, Uint32 h, const std::vector<Uint8> &px,
									 const char *name) {
	RHI::TextureDesc d;
	d.width = w;
	d.height = h;
	d.format = RHI::TextureFormat::RGBA8;
	d.usage = RHI::TextureUsage::Sampled | RHI::TextureUsage::TransferDst;
	d.debug_name = name;
	auto tex = ctx.create_texture(d);
	ctx.upload_texture_data(*tex, px.data(), static_cast<Uint64>(px.size()));
	return tex;
}

std::string ColorPicker::fmt_int(int v) {
	return std::to_string(v);
}

std::string ColorPicker::fmt_hex(Vec4 c) {
	auto ch = [](float v) { return static_cast<int>(Math::round(Math::clamp(v, 0.F, 1.F) * 255.F)); };
	std::ostringstream ss;
	ss << std::uppercase << std::hex << std::setfill('0') << std::setw(2) << ch(c.r) << std::setw(2) << ch(c.g)
	   << std::setw(2) << ch(c.b) << std::setw(2) << ch(c.a);
	return ss.str();
}

bool ColorPicker::parse_hex(const std::string &s, Vec4 &out) {
	std::string h = s;
	if (!h.empty() && h[0] == '#') {
		h = h.substr(1);
	}
	if (h.size() == 6) {
		h += "FF";
	}
	if (h.size() != 8) {
		return false;
	}
	try {
		const Uint32 v = static_cast<Uint32>(std::stoul(h, nullptr, 16));
		out.r = ((v >> 24) & 0xFF) / 255.F;
		out.g = ((v >> 16) & 0xFF) / 255.F;
		out.b = ((v >> 8) & 0xFF) / 255.F;
		out.a = ((v >> 0) & 0xFF) / 255.F;
		return true;
	} catch (...) {
		return false;
	}
}

ColorPicker::ColorPicker(GFX::GfxContext &ctx, Vec4 color) : m_ctx(ctx), m_color(color) {
	const Vec3 hsv = rgb_to_hsv(color.r, color.g, color.b);
	m_h = hsv.x;
	m_s = hsv.y;
	m_v = hsv.z;

	init();
}

void ColorPicker::set_color(Vec4 color) {
	if (m_color == color && !m_popup->is_open()) {
		return;
	}

	m_color = color;
	const Vec3 hsv = rgb_to_hsv(color.r, color.g, color.b);
	m_h = hsv.x;
	m_s = hsv.y;
	m_v = hsv.z;
	rebuild_sv_texture();
	rebuild_alpha_texture();
	sync_all();
}

void ColorPicker::on_mouse_press(Platform::MouseButton btn, Vec2 pos) {
	View::on_mouse_press(btn, pos);
}

void ColorPicker::on_draw_self(Rendering::DrawList &dl) {
	View::on_draw_self(dl);
}

void ColorPicker::rebuild_sv_texture() {
	auto px = gen_sv(m_h, K_TEX_W, K_SV_TEX_H);
	if (!m_sv_tex) {
		m_sv_tex = make_tex(m_ctx, K_TEX_W, K_SV_TEX_H, px, "ColorPicker_SV");
	} else {
		m_ctx.upload_texture_data(*m_sv_tex, px.data(), static_cast<Uint64>(px.size()));
	}
	if (m_sv_area != nullptr) {
		m_sv_area->m_tex = m_sv_tex.get();
		m_sv_area->queue_redraw();
	}
}

void ColorPicker::rebuild_alpha_texture() {
	auto px = gen_alpha({ m_color.r, m_color.g, m_color.b }, K_TEX_W, K_BAR_TEX_H);
	if (!m_alpha_tex) {
		m_alpha_tex = make_tex(m_ctx, K_TEX_W, K_BAR_TEX_H, px, "ColorPicker_Alpha");
	} else {
		m_ctx.upload_texture_data(*m_alpha_tex, px.data(), static_cast<Uint64>(px.size()));
	}
	if (m_alpha_bar != nullptr) {
		m_alpha_bar->m_tex = m_alpha_tex.get();
		m_alpha_bar->queue_redraw();
	}
}

void ColorPicker::rebuild_channel_textures() {
	const bool is_hsv = (m_mode == Mode::HSV);
	static const char *names[4] = { "ColorPicker_Ch0", "ColorPicker_Ch1", "ColorPicker_Ch2", "ColorPicker_Ch3" };
	for (int i = 0; i < 4; ++i) {
		auto px = gen_channel_grad(m_color, m_h, m_s, m_v, i, is_hsv, K_TEX_W, K_BAR_TEX_H);
		if (!m_ch_tex[i]) {
			m_ch_tex[i] = make_tex(m_ctx, K_TEX_W, K_BAR_TEX_H, px, names[i]);
		} else {
			m_ctx.upload_texture_data(*m_ch_tex[i], px.data(), static_cast<Uint64>(px.size()));
		}
		if (m_ch[i].slider != nullptr) {
			m_ch[i].slider->set_track_texture(m_ch_tex[i].get());
		}
	}
}

void ColorPicker::sync_channel_displays() {
	if (m_ch[0].slider == nullptr) {
		return;
	}

	float vals[4] = {};
	if (m_mode == Mode::RGB) {
		vals[0] = m_color.r * 255.F;
		vals[1] = m_color.g * 255.F;
		vals[2] = m_color.b * 255.F;
		vals[3] = m_color.a * 255.F;
		for (int i = 0; i < 4; ++i) {
			m_ch[i].slider->set_value_without_notify(vals[i]);
			m_ch[i].input->set_text(fmt_int(static_cast<int>(Math::round(vals[i]))));
		}
	} else if (m_mode == Mode::HSV) {
		vals[0] = m_h * 360.F;
		vals[1] = m_s * 100.F;
		vals[2] = m_v * 100.F;
		vals[3] = m_color.a * 255.F;
		for (int i = 0; i < 4; ++i) {
			m_ch[i].slider->set_value_without_notify(vals[i]);
			m_ch[i].input->set_text(fmt_int(static_cast<int>(Math::round(vals[i]))));
		}
	} else { // HEX
		m_ch[3].slider->set_value_without_notify(m_color.a * 255.F);
		m_ch[3].input->set_text(fmt_int(static_cast<int>(Math::round(m_color.a * 255.F))));
	}
}

void ColorPicker::sync_hex_display() {
	if (m_hex_input != nullptr) {
		m_hex_input->set_text(fmt_hex(m_color));
	}
}

void ColorPicker::sync_all() {
	if (m_sv_area != nullptr) {
		m_sv_area->m_indicator = { m_s, 1.F - m_v };
		m_sv_area->queue_redraw();
	}
	if (m_hue_bar != nullptr) {
		m_hue_bar->m_indicator = { m_h, 0.F };
		m_hue_bar->queue_redraw();
	}
	if (m_alpha_bar != nullptr) {
		m_alpha_bar->m_indicator = { m_color.a, 0.F };
		m_alpha_bar->queue_redraw();
	}
	if (m_preview != nullptr) {
		StyleProperties p;
		p.background_color = m_color;
		m_preview->merge_style(p);
	}
	if (m_swatch != nullptr) {
		StyleProperties p;
		p.background_color = m_color;
		m_swatch->merge_style(p);
	}
	rebuild_channel_textures();
	sync_channel_displays();
	sync_hex_display();
}

void ColorPicker::apply_channel_value(int idx, float raw_value) {
	if (m_mode == Mode::RGB) {
		const float f = raw_value / 255.F;
		switch (idx) {
		case 0:
			m_color.r = f;
			break;
		case 1:
			m_color.g = f;
			break;
		case 2:
			m_color.b = f;
			break;
		case 3:
			m_color.a = f;
			break;
		}
		if (idx < 3) {
			const Vec3 hsv = rgb_to_hsv(m_color.r, m_color.g, m_color.b);
			m_h = hsv.x;
			m_s = hsv.y;
			m_v = hsv.z;
			rebuild_sv_texture();
		}
		rebuild_alpha_texture();
	} else if (m_mode == Mode::HSV) {
		switch (idx) {
		case 0:
			m_h = raw_value / 360.F;
			break;
		case 1:
			m_s = raw_value / 100.F;
			break;
		case 2:
			m_v = raw_value / 100.F;
			break;
		case 3:
			m_color.a = raw_value / 255.F;
			break;
		}
		const Vec3 rgb = hsv_to_rgb(m_h, m_s, m_v);
		m_color.r = rgb.r;
		m_color.g = rgb.g;
		m_color.b = rgb.b;
		if (idx == 0) {
			rebuild_sv_texture();
		}
		rebuild_alpha_texture();
	} else {
		m_color.a = raw_value / 255.F;
	}

	sync_all();
	on_changed(m_color);
}

void ColorPicker::toggle_popup() {
	if (m_popup == nullptr) {
		return;
	}
	if (!m_popup->is_open()) {
		const Rect swatch_rect = m_swatch->get_absolute_rect();
		FloatingConfig fc = m_popup->get_floating();
		if (swatch_rect.position.x > m_popup->get_parent()->get_layout_rect().width() * 0.5F) {
			fc.element_point = FloatingAttachPoint::RightTop;
			fc.parent_point = FloatingAttachPoint::RightBottom;
		} else {
			fc.element_point = FloatingAttachPoint::LeftTop;
			fc.parent_point = FloatingAttachPoint::LeftBottom;
		}
		m_popup->set_floating(fc);
	}
	m_popup->toggle();
}
void ColorPicker::set_mode(Mode mode) {
	m_mode = mode;

	m_rgb_btn->remove_class("cp-mode-active");
	m_hsv_btn->remove_class("cp-mode-active");
	m_hex_btn->remove_class("cp-mode-active");
	switch (mode) {
	case Mode::RGB:
		m_rgb_btn->add_class("cp-mode-active");
		break;
	case Mode::HSV:
		m_hsv_btn->add_class("cp-mode-active");
		break;
	case Mode::HEX:
		m_hex_btn->add_class("cp-mode-active");
		break;
	}

	static const char *rgb_lbls[4] = { "R", "G", "B", "A" };
	static const char *hsv_lbls[4] = { "H", "S", "V", "A" };

	const bool is_hex = (mode == Mode::HEX);
	const bool is_hsv = (mode == Mode::HSV);

	m_hex_row->set_hidden(!is_hex);

	for (int i = 0; i < 3; ++i) {
		m_ch[i].row->set_hidden(is_hex);
	}

	if (!is_hex) {
		const char **lbls = is_hsv ? hsv_lbls : rgb_lbls;
		for (int i = 0; i < 4; ++i) {
			m_ch[i].label->set_text(lbls[i]);
		}

		if (is_hsv) {
			m_ch[0].slider->set_range(0.F, 360.F);
			m_ch[1].slider->set_range(0.F, 100.F);
			m_ch[2].slider->set_range(0.F, 100.F);
			m_ch[3].slider->set_range(0.F, 255.F);
		} else {
			for (int i = 0; i < 4; ++i) {
				m_ch[i].slider->set_range(0.F, 255.F);
			}
		}
	} else {
		m_ch[3].label->set_text("A");
		m_ch[3].slider->set_range(0.F, 255.F);
	}

	rebuild_channel_textures();
	sync_channel_displays();
	sync_hex_display();
}

void ColorPicker::init() {
	add_class("color-picker");

	{
		auto btn = std::make_unique<Button>();
		btn->add_class("cp-swatch");
		btn->on_click.connect([this] { toggle_popup(); });
		m_swatch = add_child(std::move(btn));
	}

	m_hue_tex = make_tex(m_ctx, K_TEX_W, K_BAR_TEX_H, gen_hue(K_TEX_W, K_BAR_TEX_H), "ColorPicker_Hue");
	rebuild_sv_texture();
	rebuild_alpha_texture();

	{
		FloatingConfig fc;
		fc.attach_to = FloatingAttachTo::Parent;
		fc.element_point = FloatingAttachPoint::LeftTop;
		fc.parent_point = FloatingAttachPoint::LeftBottom;
		fc.offset = { 0.F, 4.F };
		fc.z_index = 60;

		auto popup = std::make_unique<Popup>();
		popup->add_class("cp-popup");
		popup->set_floating(fc);
		m_popup = dynamic_cast<Popup *>(add_child(std::move(popup)));
	}

	{
		auto sv = std::make_unique<PickerArea>();
		sv->add_class("cp-sv-area");
		sv->m_tex = m_sv_tex.get();
		sv->m_is1_d = false;
		sv->m_indicator = { m_s, 1.F - m_v };
		sv->m_on_pick = [this](Vec2 n) {
			m_s = n.x;
			m_v = 1.F - n.y;
			const Vec3 rgb = hsv_to_rgb(m_h, m_s, m_v);
			m_color.r = rgb.r;
			m_color.g = rgb.g;
			m_color.b = rgb.b;
			rebuild_alpha_texture();
			sync_all();
			on_changed(m_color);
		};
		m_sv_area = dynamic_cast<PickerArea *>(m_popup->add_child(std::move(sv)));
	}

	{
		auto row = std::make_unique<View>();
		row->add_class("cp-hue-row");
		View *row_raw = m_popup->add_child(std::move(row));

		{
			auto hue = std::make_unique<PickerArea>();
			hue->add_class("cp-hue-bar");
			hue->m_tex = m_hue_tex.get();
			hue->m_is1_d = true;
			hue->m_indicator = { m_h, 0.F };
			hue->m_on_pick = [this](Vec2 n) {
				const bool changed = Math::abs(n.x - m_h) > 1e-4F;
				m_h = n.x;
				const Vec3 rgb = hsv_to_rgb(m_h, m_s, m_v);
				m_color.r = rgb.r;
				m_color.g = rgb.g;
				m_color.b = rgb.b;
				if (changed) {
					rebuild_sv_texture();
				}
				rebuild_alpha_texture();
				sync_all();
				on_changed(m_color);
			};
			m_hue_bar = dynamic_cast<PickerArea *>(row_raw->add_child(std::move(hue)));
		}

		{
			auto prev = std::make_unique<View>();
			prev->add_class("cp-preview");
			m_preview = row_raw->add_child(std::move(prev));
		}
	}

	{
		auto alpha = std::make_unique<PickerArea>();
		alpha->add_class("cp-alpha-bar");
		alpha->m_tex = m_alpha_tex.get();
		alpha->m_is1_d = true;
		alpha->m_indicator = { m_color.a, 0.F };
		alpha->m_on_pick = [this](Vec2 n) {
			m_color.a = n.x;
			sync_all();
			on_changed(m_color);
		};
		m_alpha_bar = dynamic_cast<PickerArea *>(m_popup->add_child(std::move(alpha)));
	}

	{
		auto row = std::make_unique<View>();
		row->add_class("cp-mode-row");
		View *row_raw = m_popup->add_child(std::move(row));

		auto make_btn = [&](const char *text) -> Button * {
			auto btn = std::make_unique<Button>(text);
			btn->add_class("cp-mode-btn");
			return dynamic_cast<Button *>(row_raw->add_child(std::move(btn)));
		};
		m_rgb_btn = make_btn("RGB");
		m_hsv_btn = make_btn("HSV");
		m_hex_btn = make_btn("HEX");
		m_rgb_btn->on_click.connect([this] { set_mode(Mode::RGB); });
		m_hsv_btn->on_click.connect([this] { set_mode(Mode::HSV); });
		m_hex_btn->on_click.connect([this] { set_mode(Mode::HEX); });
	}

	const char *labels[4] = { "R", "G", "B", "A" };
	for (int i = 0; i < 4; ++i) {
		auto row = std::make_unique<View>();
		row->add_class("cp-ch-row");
		m_ch[i].row = m_popup->add_child(std::move(row));

		{
			auto lbl = std::make_unique<Label>(labels[i]);
			lbl->add_class("cp-ch-lbl");
			m_ch[i].label = dynamic_cast<Label *>(m_ch[i].row->add_child(std::move(lbl)));
		}
		{
			auto sl = std::make_unique<Slider>();
			sl->set_range(0.F, 255.F);
			sl->set_step(1.F);
			sl->add_class("cp-ch-slider");
			m_ch[i].slider = dynamic_cast<Slider *>(m_ch[i].row->add_child(std::move(sl)));
		}
		{
			auto ti = std::make_unique<TextInput>();
			ti->add_class("cp-ch-input");
			m_ch[i].input = dynamic_cast<TextInput *>(m_ch[i].row->add_child(std::move(ti)));
		}

		const int idx = i;
		m_ch[i].slider->on_changed.connect([this, idx](float val) {
			m_ch[idx].input->set_text(fmt_int(static_cast<int>(Math::round(val))));
			apply_channel_value(idx, val);
		});
		m_ch[i].input->on_submit.connect([this, idx](const std::string &s) {
			try {
				const float val = static_cast<float>(std::stoi(s));
				m_ch[idx].slider->set_value_without_notify(val);
				apply_channel_value(idx, val);
			} catch (...) {
			}
		});
	}

	{
		auto row = std::make_unique<View>();
		row->add_class("cp-hex-row");
		m_hex_row = m_popup->add_child(std::move(row));

		{
			auto lbl = std::make_unique<Label>("#");
			lbl->add_class("cp-hex-lbl");
			m_hex_row->add_child(std::move(lbl));
		}
		{
			auto ti = std::make_unique<TextInput>();
			ti->add_class("cp-hex-input");
			ti->on_submit.connect([this](const std::string &s) {
				Vec4 c;
				if (parse_hex(s, c)) {
					m_color = c;
					const Vec3 hsv = rgb_to_hsv(c.r, c.g, c.b);
					m_h = hsv.x;
					m_s = hsv.y;
					m_v = hsv.z;
					rebuild_sv_texture();
					rebuild_alpha_texture();
					sync_all();
					on_changed(m_color);
				}
			});
			m_hex_input = dynamic_cast<TextInput *>(m_hex_row->add_child(std::move(ti)));
		}
	}

	m_rgb_btn->add_class("cp-mode-active");
	rebuild_channel_textures();
	sync_all();
}

} // namespace Aquila::UI::Core
