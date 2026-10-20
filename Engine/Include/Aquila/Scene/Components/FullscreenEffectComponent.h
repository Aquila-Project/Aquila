#ifndef AQUILA_SCENE_FULLSCREEN_EFFECT_COMPONENT_H
#define AQUILA_SCENE_FULLSCREEN_EFFECT_COMPONENT_H

#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Aquila::SceneManagement::Components {

/// Runs a fullscreen material (a .aqmat whose shader imports Aquila.Fullscreen) over the lit image, before tone
/// mapping, so it sees exposed HDR colors. Effects run from the lowest `order` up, each one reads what the previous
/// wrote. Any entity can carry one.
struct FullscreenEffectComponent {
	std::string material;
	Int32 order = 0;
	bool active = true;
};

}

#endif
