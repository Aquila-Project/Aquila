#ifndef AQUILA_RENDERING_RENDER_SETTINGS_H
#define AQUILA_RENDERING_RENDER_SETTINGS_H

#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Aquila::Rendering {

enum class DebugView : Uint32 { Lit, Albedo, Normals, Depth, Wireframe, ShadowCascades, LightComplexity };

struct RenderSettings {
	DebugView debug_view = DebugView::Lit;
	bool show_grid = true;
	bool show_sky = true;
	bool show_shadows = true;
	bool show_outline = true;
	/// Render every frame even when nothing changed, like Godot's "Update Continuously".
	bool realtime = false;
};

} // namespace Aquila::Rendering

#endif
