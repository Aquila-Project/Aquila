#ifndef AQUILA_SHADOW_DATA_SLANG
#define AQUILA_SHADOW_DATA_SLANG

import FrameData;

namespace Aquila::Shading {

struct ShadowData {
	column_major float4x4 cascadeViewProj[4];
	float4 cascadeSplits;
	float4 lightDirection;
	float4 params; // x = depth bias, y = normal bias, z = pcf texel scale, w = shadow map size
	int enabled;
	int numCascades;
	int2 _pad;
};

[vk::binding(6, 0)] ConstantBuffer<ShadowData> shadowData;
[vk::binding(7, 0)] Sampler2D shadowMap0;
[vk::binding(8, 0)] Sampler2D shadowMap1;
[vk::binding(9, 0)] Sampler2D shadowMap2;
[vk::binding(10, 0)] Sampler2D shadowMap3;

static const int SHADOW_BLOCKER_SAMPLES = 12;
static const int SHADOW_FILTER_SAMPLES = 16;

float4 GatherCascade(int cascade, float2 uv) {
	if (cascade == 0) {
		return shadowMap0.Gather(uv);
	} else if (cascade == 1) {
		return shadowMap1.Gather(uv);
	} else if (cascade == 2) {
		return shadowMap2.Gather(uv);
	}
	return shadowMap3.Gather(uv);
}

float ShadowNoise(float2 pixel) {
	return frac(52.9829189 * frac(dot(pixel, float2(0.06711056, 0.00583715))));
}

float2 VogelDisk(int index, int count, float rotation) {
	float r = sqrt((float(index) + 0.5) / float(count));
	float theta = float(index) * 2.39996323 + rotation;
	return r * float2(cos(theta), sin(theta));
}

float BilinearCompare(int cascade, float2 uv, float depthRef, float mapSize) {
	float2 texelPos = uv * mapSize - 0.5;
	float2 f = frac(texelPos);
	float4 depths = GatherCascade(cascade, (floor(texelPos) + 1.0) / mapSize);
	float4 lit = step(float4(depthRef), depths);
	float top = lerp(lit.w, lit.z, f.x);
	float bottom = lerp(lit.x, lit.y, f.x);
	return lerp(top, bottom, f.y);
}

float SampleCascadeVisibility(int cascade, float3 worldPos, float3 N, float3 L, float rotation) {
	float NdotL = saturate(dot(N, L));
	float slope = saturate(1.0 - NdotL);
	float3 offsetPos = worldPos + N * shadowData.params.y * (1.0 + float(cascade)) * (0.5 + slope);

	float4 lightSpace = mul(transpose(shadowData.cascadeViewProj[cascade]), float4(offsetPos, 1.0));
	float3 proj = lightSpace.xyz / lightSpace.w;
	float2 uv = proj.xy * 0.5 + 0.5;
	float depthRef = proj.z;

	if (depthRef <= 0.0 || depthRef >= 1.0) {
		return 1.0;
	}
	if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
		return 1.0;
	}

	depthRef -= shadowData.params.x * (1.0 + float(cascade));

	float mapSize = shadowData.params.w;
	float texel = 1.0 / mapSize;
	float lightTan = shadowData.params.z;
	float tanSlope = min(sqrt(max(1.0 - NdotL * NdotL, 0.0)) / max(NdotL, 0.05), 8.0);

	float searchRadius = clamp(depthRef * 2.0 * lightTan, 1.5 * texel, 32.0 * texel);
	float blockerSum = 0.0;
	float blockerCount = 0.0;
	for (int i = 0; i < SHADOW_BLOCKER_SAMPLES; ++i) {
		float2 offset = VogelDisk(i, SHADOW_BLOCKER_SAMPLES, rotation) * searchRadius;
		float tapRef = depthRef - length(offset) * tanSlope * 0.5;
		float4 depths = GatherCascade(cascade, uv + offset);
		float4 blocked = float4(1.0) - step(float4(tapRef), depths);
		blockerSum += dot(blocked, depths);
		blockerCount += dot(blocked, float4(1.0));
	}
	if (blockerCount < 0.5) {
		return 1.0;
	}

	float blockerDepth = blockerSum / blockerCount;
	float penumbra = clamp((depthRef - blockerDepth) * 2.0 * lightTan, texel, searchRadius);

	float sum = 0.0;
	for (int i = 0; i < SHADOW_FILTER_SAMPLES; ++i) {
		float2 offset = VogelDisk(i, SHADOW_FILTER_SAMPLES, rotation) * penumbra;
		float tapRef = depthRef - length(offset) * tanSlope * 0.5;
		sum += BilinearCompare(cascade, uv + offset, tapRef, mapSize);
	}
	return sum / float(SHADOW_FILTER_SAMPLES);
}

float ComputeDirectionalShadow(float3 worldPos, float3 N, float3 L, float2 pixel) {
	if (shadowData.enabled == 0) {
		return 1.0;
	}

	float viewZ = mul(GetMainCamera().view, float4(worldPos, 1.0)).z;

	int cascade = -1;
	for (int i = 0; i < shadowData.numCascades; ++i) {
		if (viewZ <= shadowData.cascadeSplits[i]) {
			cascade = i;
			break;
		}
	}
	if (cascade < 0) {
		return 1.0;
	}

	float rotation = ShadowNoise(pixel) * 6.28318530718;
	float visibility = SampleCascadeVisibility(cascade, worldPos, N, L, rotation);

	float splitFar = shadowData.cascadeSplits[cascade];
	float splitNear = (cascade == 0) ? 0.0 : shadowData.cascadeSplits[cascade - 1];
	float bandStart = lerp(splitNear, splitFar, 0.8);
	if (cascade + 1 < shadowData.numCascades && viewZ > bandStart) {
		float t = saturate((viewZ - bandStart) / max(splitFar - bandStart, 1e-4));
		float nextVisibility = SampleCascadeVisibility(cascade + 1, worldPos, N, L, rotation);
		visibility = lerp(visibility, nextVisibility, t);
	}

	return visibility;
}

struct LocalShadow {
	float4 viewProj0;
	float4 viewProj1;
	float4 viewProj2;
	float4 viewProj3;
	float4 rect;
	float4 params;
	float4 extra;
};

[vk::binding(11, 0)] StructuredBuffer<LocalShadow> localShadows;
[vk::binding(12, 0)] Sampler2D localShadowAtlas;

static const int LOCAL_BLOCKER_SAMPLES = 8;
static const int LOCAL_FILTER_SAMPLES = 12;
static const float LOCAL_MAX_KERNEL_TEXELS = 14.0;

int CubeFace(float3 v) {
	float3 a = abs(v);
	if (a.x >= a.y && a.x >= a.z) {
		return v.x > 0.0 ? 0 : 1;
	}
	if (a.y >= a.z) {
		return v.y > 0.0 ? 2 : 3;
	}
	return v.z > 0.0 ? 4 : 5;
}

float4 LinearizeLocalDepth(float4 depth, float near, float far) {
	return float4(near * far) / (float4(far) - depth * (far - near));
}

float LocalBilinearCompare(float2 atlasUV, float receiver, float atlasSize, float near, float far) {
	float2 texelPos = atlasUV * atlasSize - 0.5;
	float2 f = frac(texelPos);
	float4 depths = LinearizeLocalDepth(localShadowAtlas.Gather((floor(texelPos) + 1.0) / atlasSize), near, far);
	float4 lit = step(float4(receiver), depths);
	float top = lerp(lit.w, lit.z, f.x);
	float bottom = lerp(lit.x, lit.y, f.x);
	return lerp(top, bottom, f.y);
}

float ComputeLocalShadow(int shadowIndex, bool isPoint, float3 lightPos, float3 worldPos, float3 N, float3 L,
						 float2 pixel) {
	int index = shadowIndex;
	if (isPoint) {
		index += CubeFace(worldPos - lightPos);
	}
	LocalShadow s = localShadows[index];
	float near = s.params.x;
	float far = s.params.y;
	float lightRadius = s.params.z;
	float cotHalf = s.params.w;
	float tileSize = s.extra.x;
	float atlasSize = s.extra.y;

	float NdotL = saturate(dot(N, L));
	float tanSlope = min(sqrt(max(1.0 - NdotL * NdotL, 0.0)) / max(NdotL, 0.05), 8.0);
	float texelWorld = 2.0 * length(worldPos - lightPos) / (cotHalf * tileSize);
	float3 offsetPos = worldPos + N * texelWorld * (1.0 + 0.5 * tanSlope);

	float4 clip = s.viewProj0 * offsetPos.x + s.viewProj1 * offsetPos.y + s.viewProj2 * offsetPos.z + s.viewProj3;
	if (clip.w <= 1e-4) {
		return 1.0;
	}
	float3 ndc = clip.xyz / clip.w;
	if (abs(ndc.x) > 1.0 || abs(ndc.y) > 1.0 || ndc.z <= 0.0 || ndc.z >= 1.0) {
		return 1.0;
	}

	float2 tileUV = ndc.xy * 0.5 + 0.5;
	float receiver = clip.w;
	float tileTexel = 1.0 / tileSize;
	float uvPerMeter = 0.5 * cotHalf / receiver;
	float bias = texelWorld * 1.5;
	float maxRadius = LOCAL_MAX_KERNEL_TEXELS * tileTexel;
	float rotation = ShadowNoise(pixel) * 6.28318530718;

	float searchRadius = clamp(lightRadius * cotHalf / receiver, 1.5 * tileTexel, maxRadius);
	float blockerSum = 0.0;
	float blockerCount = 0.0;
	for (int i = 0; i < LOCAL_BLOCKER_SAMPLES; ++i) {
		float2 offset = VogelDisk(i, LOCAL_BLOCKER_SAMPLES, rotation) * searchRadius;
		float2 uv = clamp(tileUV + offset, float2(tileTexel), float2(1.0 - tileTexel));
		float tapReceiver = receiver - bias - length(offset) / uvPerMeter * tanSlope;
		float4 depths = LinearizeLocalDepth(localShadowAtlas.Gather(s.rect.xy + uv * s.rect.zw), near, far);
		float4 blocked = float4(1.0) - step(float4(tapReceiver), depths);
		blockerSum += dot(blocked, depths);
		blockerCount += dot(blocked, float4(1.0));
	}
	if (blockerCount < 0.5) {
		return 1.0;
	}

	float blocker = blockerSum / blockerCount;
	float penumbra = clamp(lightRadius * (receiver - blocker) / max(blocker, 1e-3) * uvPerMeter, tileTexel, maxRadius);

	float sum = 0.0;
	for (int i = 0; i < LOCAL_FILTER_SAMPLES; ++i) {
		float2 offset = VogelDisk(i, LOCAL_FILTER_SAMPLES, rotation) * penumbra;
		float2 uv = clamp(tileUV + offset, float2(tileTexel), float2(1.0 - tileTexel));
		float tapReceiver = receiver - bias - length(offset) / uvPerMeter * tanSlope;
		sum += LocalBilinearCompare(s.rect.xy + uv * s.rect.zw, tapReceiver, atlasSize, near, far);
	}
	return sum / float(LOCAL_FILTER_SAMPLES);
}

float3 CascadeDebugColor(float3 worldPos) {
	float viewZ = mul(GetMainCamera().view, float4(worldPos, 1.0)).z;
	int cascade = -1;
	for (int i = 0; i < shadowData.numCascades; ++i) {
		if (viewZ <= shadowData.cascadeSplits[i]) {
			cascade = i;
			break;
		}
	}
	if (cascade == 0) {
		return float3(1.0, 0.3, 0.3);
	}
	if (cascade == 1) {
		return float3(0.3, 1.0, 0.3);
	}
	if (cascade == 2) {
		return float3(0.3, 0.3, 1.0);
	}
	if (cascade == 3) {
		return float3(1.0, 1.0, 0.3);
	}
	return float3(0.3, 0.3, 0.3);
}

} // namespace Aquila::Shading

#endif // AQUILA_SHADOW_DATA_SLANG
