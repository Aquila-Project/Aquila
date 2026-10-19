#ifndef AQUILA_RENDERING_RENDER_SETTINGS_H
#define AQUILA_RENDERING_RENDER_SETTINGS_H

#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Aquila::Rendering {

enum class DebugView : Uint32 { Lit, Albedo, Normals, Depth, Wireframe, ShadowCascades, LightComplexity };

/// How the exposed HDR image gets squeezed into the screen's 0..1. None just clips.
enum class ToneMapper : Uint32 { None, Aces, AgX, Neutral };

/// "none", "aces", "agx", "neutral", for settings files.
[[nodiscard]] inline std::string_view tone_mapper_name(ToneMapper mapper) {
	switch (mapper) {
	case ToneMapper::None:
		return "none";
	case ToneMapper::Aces:
		return "aces";
	case ToneMapper::Neutral:
		return "neutral";
	case ToneMapper::AgX:
		break;
	}
	return "agx";
}

/// AgX for anything it doesn't know.
[[nodiscard]] inline ToneMapper tone_mapper_from_name(std::string_view name) {
	if (name == "none") {
		return ToneMapper::None;
	}
	if (name == "aces") {
		return ToneMapper::Aces;
	}
	if (name == "neutral") {
		return ToneMapper::Neutral;
	}
	return ToneMapper::AgX;
}

struct RenderSettings {
	DebugView debug_view = DebugView::Lit;
	bool show_grid = true;
	bool show_sky = true;
	bool show_shadows = true;
	bool show_outline = true;
	/// Only for the Lit view, debug views show their values as they are.
	ToneMapper tone_mapper = ToneMapper::AgX;
	/// Render every frame even when nothing changed, like Godot's "Update Continuously".
	bool realtime = false;

	/// What a built game draws with: lit, no grid, no selection outline. The editor's Game view uses it too.
	[[nodiscard]] static RenderSettings game() { return { .show_grid = false, .show_outline = false }; }
};

} // namespace Aquila::Rendering

#endif
