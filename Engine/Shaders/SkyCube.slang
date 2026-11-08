#ifndef AQUILA_SKY_CUBE_SLANG
#define AQUILA_SKY_CUBE_SLANG

import Utils;
import Utility.SkyData;
import Utility.CubeMap;

using namespace Aquila::Shading;

static const float LUMINOUS_EFFICACY = 683.0;

struct SkyCubePush {
	uint face;
};

[[vk::push_constant]]
SkyCubePush push;

[[vk::binding(0, 1)]]
ConstantBuffer<SkyData> sky;

struct VSOutput {
	float4 position : SV_Position;
	float2 uv : TEXCOORD0;
};

[shader("vertex")]
VSOutput vertexMain(uint vertexIndex: SV_VertexID) {
	VSOutput OUT;
	OUT.position = FullscreenVertexPosition(vertexIndex);
	OUT.uv = FullscreenTexCoord(vertexIndex);
	return OUT;
}

[shader("fragment")]
float4 fragmentMain(VSOutput IN) : SV_Target {
	float3 dir = CubeFaceDirection(push.face, IN.uv);
	return float4(SkyRadiance(sky, dir) * LUMINOUS_EFFICACY, 1.0);
}

#endif
