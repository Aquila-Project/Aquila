#ifndef AQUILA_SKY_IMAGE_SLANG
#define AQUILA_SKY_IMAGE_SLANG

import Utility.FrameData;
import Utility.EnvironmentData;
import Utility.SkyData;
import Utility.Equirect;
import "Utils.slang";

using namespace Aquila::Shading;

[[vk::binding(0, 1)]] ConstantBuffer<SkyData> sky;
[[vk::binding(1, 1)]] Sampler2D skyImage;

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

	uint width, height, levels;
	skyImage.GetDimensions(0, width, height, levels);
	float pixelAngle = sqrt(max(dot(ddx(dir), ddx(dir)), dot(ddy(dir), ddy(dir))));
	float lod = log2(max(pixelAngle * float(height) * INV_PI, 1e-6));
	float3 color = skyImage.SampleLevel(EquirectUV(dir), lod).rgb * sky.tintIntensity.w * sky.tintIntensity.xyz;

	float3 cameraPos = GetCameraPosition();
	color = ApplyFog(color, cameraPos, cameraPos + dir * 100000.0);
	return float4(color * GetExposure(), 1.0);
}

#endif
