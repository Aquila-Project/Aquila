#ifndef AQUILA_SCENE_OUTLINE_COMPONENT_H
#define AQUILA_SCENE_OUTLINE_COMPONENT_H

#include "Aquila/Foundation/Math/MathTypes.h"
#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Aquila::SceneManagement::Components {

/// Add it to draw an outline around the entity. The editor adds it to the selection.
struct OutlineComponent {
	Vec4 color{ 0.290F, 0.620F, 0.373F, 1.F };
	F32 thickness = 2.F;
};

}

#endif
