#ifndef AQUILA_DEPTH_ONLY_SLANG
#define AQUILA_DEPTH_ONLY_SLANG

import Utility.FrameData;

using namespace Aquila::Shading;

struct PushConstants {
	column_major float4x4 model;
};

[[vk::push_constant]]
PushConstants push;

struct VSInput {
	[[vk::location(0)]]
	float3 position : POSITION;
};

[shader("vertex")]
float4 main(VSInput IN) : SV_Position {
	return mul(GetMainCamera().viewProjection, mul(push.model, float4(IN.position, 1.0)));
}

#endif // AQUILA_DEPTH_ONLY_SLANG
