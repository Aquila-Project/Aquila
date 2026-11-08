#ifndef AQUILA_SKY_PREFILTER_SLANG
#define AQUILA_SKY_PREFILTER_SLANG

import Utils;
import Utility.CubeMap;
import Utility.Sampling;

using namespace Aquila::Shading;

static const uint SAMPLE_COUNT = 64;

struct PrefilterPush {
	uint face;
	float roughness;
	float sourceSize;
	float baseLod;
};

[[vk::push_constant]]
PrefilterPush push;

[[vk::binding(0, 1)]]
SamplerCube skyCube;

struct VSOutput {
	float4 position : SV_Position;
	float2 uv : TEXCOORD0;
};

float DistributionGGX(float NoH, float a) {
	float a2 = a * a;
	float d = NoH * NoH * (a2 - 1.0) + 1.0;
	return a2 / (PI * d * d);
}

float3 PrefilterEnvMap(float roughness, float3 R) {
	float3 N = R;
	float3 V = R;
	float a = roughness * roughness;
	float texelSolidAngle = 4.0 * PI / (6.0 * push.sourceSize * push.sourceSize);

	float3 prefiltered = float3(0.0);
	float totalWeight = 0.0;
	for (uint i = 0; i < SAMPLE_COUNT; i++) {
		float2 Xi = Hammersley(i, SAMPLE_COUNT);
		float3 H = ImportanceSampleGGX(Xi, roughness, N);
		float3 L = 2.0 * dot(V, H) * H - V;

		float NoL = saturate(dot(N, L));
		if (NoL > 0.0) {
			float pdf = DistributionGGX(saturate(dot(N, H)), a) * 0.25;
			float sampleSolidAngle = 1.0 / (float(SAMPLE_COUNT) * pdf + 1e-4);
			float lod = max(0.5 * log2(sampleSolidAngle / texelSolidAngle) + 1.0, push.baseLod);
			prefiltered += skyCube.SampleLevel(L, lod).rgb * NoL;
			totalWeight += NoL;
		}
	}
	return prefiltered / max(totalWeight, 1e-4);
}

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

	if (push.roughness == 0.0) {
		return float4(skyCube.SampleLevel(dir, push.baseLod).rgb, 1.0);
	}
	return float4(PrefilterEnvMap(push.roughness, dir), 1.0);
}

#endif
