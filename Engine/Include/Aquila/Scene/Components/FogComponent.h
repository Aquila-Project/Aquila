#ifndef AQUILA_SCENE_FOG_COMPONENT_H
#define AQUILA_SCENE_FOG_COMPONENT_H

#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Aquila::SceneManagement::Components {

/// Exponential height fog, thick near the ground and thinning out with height. It's lit by the sky light and the
/// first directional light, so it follows the scene's light levels. One per scene, the first active one wins.
///
/// `density` is the extinction per meter at `base_height`: 0.02 hides things at about 150 m, 0.2 at 15 m.
/// `height_falloff` is per meter: how fast it thins out going up. `color` is the scattering albedo (white for real
/// fog and mist). `anisotropy` is the Henyey-Greenstein g: 0 scatters evenly, close to 1 glows around the sun.
/// `max_opacity` caps how much it can hide, `start_distance` keeps it away from the camera.
struct FogComponent {
	F32 density = 0.02F;
	F32 height_falloff = 0.2F;
	F32 base_height = 0.F;
	Vec3 color = Vec3(0.92F, 0.94F, 0.96F);
	F32 anisotropy = 0.4F;
	F32 max_opacity = 1.F;
	F32 start_distance = 0.F;
	bool active = true;
};

}

#endif
