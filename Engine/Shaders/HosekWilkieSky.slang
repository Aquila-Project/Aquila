#ifndef AQUILA_HOSEK_SKY_SLANG
#define AQUILA_HOSEK_SKY_SLANG

import Utility.FrameData;
import "Utils.slang";

using namespace Aquila::Shading;

struct SkyData {
	float4 configA[3];
	float4 configB[3];
	float4 configC[3];
	float4 sunDirection;
	float4 tintIntensity;
	float4 groundAlbedo;
};

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

float HosekRadiance(float4 a, float4 b, float c8, float theta, float gamma) {
	float cosGamma = cos(gamma);
	float cosTheta = cos(theta);
	float expM = exp(b.x * gamma);
	float rayM = cosGamma * cosGamma;
	float mieM = (1.0 + cosGamma * cosGamma) / pow(1.0 + c8 * c8 - 2.0 * c8 * cosGamma, 1.5);
	float zenith = sqrt(max(cosTheta, 0.0));

	return (1.0 + a.x * exp(a.y / (cosTheta + 0.01))) *
		   (a.z + a.w * expM + b.y * rayM + b.z * mieM + b.w * zenith);
}

[shader("fragment")]
float4 fragmentMain(VSOutput IN) : SV_Target {
	float3 nearPoint = ClipToWorld(float4(IN.ndc, 0.0, 1.0));
	float3 farPoint = ClipToWorld(float4(IN.ndc, 1.0, 1.0));
	float3 dir = normalize(farPoint - nearPoint);

	float3 sunDir = normalize(sky.sunDirection.xyz);

	float theta = acos(clamp(dir.y, 0.0, 1.0));
	float gamma = acos(clamp(dot(dir, sunDir), -1.0, 1.0));

	float3 color;
	color.r = HosekRadiance(sky.configA[0], sky.configB[0], sky.configC[0].x, theta, gamma) * sky.configC[0].y;
	color.g = HosekRadiance(sky.configA[1], sky.configB[1], sky.configC[1].x, theta, gamma) * sky.configC[1].y;
	color.b = HosekRadiance(sky.configA[2], sky.configB[2], sky.configC[2].x, theta, gamma) * sky.configC[2].y;

	color = max(color * sky.tintIntensity.w, float3(0.0));
	color *= sky.tintIntensity.xyz;

	float3 groundColor = color * sky.groundAlbedo.xyz;
	float horizonBlend = smoothstep(-0.02, 0.02, dir.y);
	color = lerp(groundColor, color, horizonBlend);

	return float4(color * GetExposure(), 1.0);
}

#endif // AQUILA_HOSEK_SKY_SLANG
