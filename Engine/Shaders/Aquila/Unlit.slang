#ifndef AQUILA_UNLIT_SLANG
#define AQUILA_UNLIT_SLANG

import Utility.FrameData;
import Aquila.Surface;

using namespace Aquila::Shading;

float3 Emission(float3 nits) {
	return nits * GetExposure();
}

#endif
