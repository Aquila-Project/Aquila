#ifndef AQUILA_RENDERING_MESH_PUSH_CONSTANTS_H
#define AQUILA_RENDERING_MESH_PUSH_CONSTANTS_H

#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Aquila::Rendering {

/// What every mesh material gets, has to match AquilaMeshPush in Aquila/LitMain.slang and UnlitMain.slang.
/// `alpha_cutoff` below 0 keeps every pixel, `transparent` keeps the shader's alpha instead of writing 1.
struct MeshPushConstants {
	Mat4 model = Mat4(1.F);
	Vec4 color = Vec4(1.F);
	Uint32 material_index = 0;
	Uint32 receives_shadows = 0;
	F32 alpha_cutoff = -1.F;
	Uint32 transparent = 0;
};

/// A masked material's shadow pass, has to match AquilaDepthPush in Aquila/LitDepth.slang and UnlitDepth.slang.
struct DepthPushConstants {
	Mat4 model = Mat4(1.F);
	Mat4 view_projection = Mat4(1.F);
	F32 alpha_cutoff = -1.F;
};

}

#endif
