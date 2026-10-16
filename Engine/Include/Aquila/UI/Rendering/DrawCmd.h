#pragma once

#include "Aquila/Foundation/Math/Rect.h"
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/UI/Style/StyleTypes.h"

#include "Aquila/GFX/GfxTexture.h"

namespace Aquila::UI::Text {
class FontAtlas;
}

namespace Aquila::UI::Rendering {

struct RectCmd {
	Rect rect;
	Vec4 color = Vec4(1.F);
	Vec4 radius = Vec4(0.F);
	float border_width = 0.F;
	Vec4 border_color = Vec4(0.F);
	BorderStyle border_style = BorderStyle::Solid;
	float rotation = 0.F;
	Int32 z_order = 0;
};

struct ShadowCmd {
	Rect rect; // the widget rect the shadow is cast from
	Vec4 color = Vec4(0.F);
	Vec4 radius = Vec4(0.F);
	Vec2 offset = { 0.F, 0.F };
	Vec2 original_half_size = { 0.F, 0.F }; // widget half-size + spread, drives the SDF
	float blur = 0.F;
	Int32 z_order = 0;
};

struct ImageCmd {
	Rect rect;
	GFX::GfxTexture *texture = nullptr;
	Vec4 tint = Vec4(0.F);
	Vec2 uv_min = { 0.F, 0.F };
	Vec2 uv_max = { 1.F, 1.F };
	Int32 z_order = 0;
};

struct TextCmd {
	Rect rect; // layout bounds for alignment
	Vec4 color = Vec4(1.F);
	std::string text;
	Text::FontAtlas *font = nullptr;
	float font_size = 0.F;
	TextAlign align = TextAlign::Left;
	bool wrap = false; // word-wrap within rect.size.x across multiple lines
	Int32 z_order = 0;
};

struct PointCmd {
	Vec2 position = DEFAULT_VALUE;
	Vec4 color = DEFAULT_VALUE;
	F32 size = 1.F;
	Int32 z_order = 0;
};

struct ClipPushCmd {
	Rect rect;
	Int32 z_order = 0;
};

struct ClipPopCmd {
	Rect rect; // rect to restore to; empty means the full canvas
	Int32 z_order = 0;
};

using DrawCmd = std::variant<RectCmd, ShadowCmd, ImageCmd, TextCmd, PointCmd, ClipPushCmd, ClipPopCmd>;

[[nodiscard]] inline Int32 draw_cmd_z_order(const DrawCmd &cmd) {
	return std::visit([](const auto &c) { return c.z_order; }, cmd);
}

} // namespace Aquila::UI::Rendering
