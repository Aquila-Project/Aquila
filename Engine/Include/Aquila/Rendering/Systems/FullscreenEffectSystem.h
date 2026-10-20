#ifndef AQUILA_RENDERING_FULLSCREEN_EFFECT_SYSTEM_H
#define AQUILA_RENDERING_FULLSCREEN_EFFECT_SYSTEM_H

#include "Aquila/Rendering/Systems/RenderingSystemBase.h"

namespace Aquila::Rendering {

/// Draws every active FullscreenEffectComponent's material over the scene color, lowest order first, each into a
/// new HDR target that becomes the scene color for the next. The material gets the current scene color and depth as
/// `sceneColor`/`sceneDepth` (Aquila/Fullscreen.slang). Add it after the sky and before ToneMapSystem.
/// @note A material used by two effects in the same frame only runs once, its descriptor set can't hold two inputs.
class FullscreenEffectSystem : public RenderingSystemBase {
  public:
	FullscreenEffectSystem() = default;
	~FullscreenEffectSystem() override = default;

	void add_passes(Graphics::RG::RenderGraph &graph, FrameContext &ctx) override;
};

}

#endif
