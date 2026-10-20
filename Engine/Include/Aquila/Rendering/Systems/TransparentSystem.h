#ifndef AQUILA_RENDERING_TRANSPARENT_SYSTEM_H
#define AQUILA_RENDERING_TRANSPARENT_SYSTEM_H

#include "Aquila/Rendering/Systems/RenderingSystemBase.h"

namespace Aquila::Rendering {

/// Draws meshes whose material is Transparent, farthest first (by object origin), blended over the scene color with
/// depth test but no depth writes. Add it after SkySystem so they blend over the sky, and before fullscreen effects.
/// @note Sorting is per object, so two intersecting transparent objects can still show in the wrong order.
class TransparentSystem : public RenderingSystemBase {
  public:
	TransparentSystem() = default;
	~TransparentSystem() override = default;

	void add_passes(Graphics::RG::RenderGraph &graph, FrameContext &ctx) override;
};

}

#endif
