#ifndef AQUILA_HOSEK_SKY_SLANG
#define AQUILA_HOSEK_SKY_SLANG

import Utility.FrameData;
import Utility.EnvironmentData;
import Utility.SkyData;
import "Utils.slang";

using namespace Aquila::Shading;

[[vk::binding(0, 1)]] ConstantBuffer<SkyData> sky;

struct VSOutput {
	float4 position : SV_Position;
	float2 ndc : TEXCOORD0;
};

[shader("vertex")]
VSOutput vertexMain(uint vertexIndex : SV_VertexID) {
	VSOutput o;
	float2 pos = FullscreenTrianglePositions[vertexIndex];
	o.position = float4(pos, 1.0, 1.0);
	o.ndc = pos;
	return o;
}

[shader("fragment")]
float4 fragmentMain(VSOutput IN) : SV_Target {
	float3 nearPoint = ClipToWorld(float4(IN.ndc, 0.0, 1.0));
	float3 farPoint = ClipToWorld(float4(IN.ndc, 1.0, 1.0));
	float3 dir = normalize(farPoint - nearPoint);

	float3 color = SkyRadiance(sky, dir) * sky.tintIntensity.w * sky.tintIntensity.xyz;

	float3 cameraPos = GetCameraPosition();
	color = ApplyFog(color, cameraPos, cameraPos + dir * 100000.0);
	return float4(color * GetExposure(), 1.0);
}

#endif // AQUILA_HOSEK_SKY_SLANG
