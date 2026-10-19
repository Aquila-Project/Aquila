#ifndef AQUILA_DEBUG_VIEW_SLANG
#define AQUILA_DEBUG_VIEW_SLANG

import Utility.FrameData;
import Utility.LightData;
import Utility.SurfaceData;
import Utility.ClusterData;
import Utility.EnvironmentData;
import Utility.ShadowData;
import "Utils.slang";

using namespace Aquila::Shading;

static const uint MODE_ALBEDO = 1;
static const uint MODE_NORMALS = 2;
static const uint MODE_DEPTH = 3;
static const uint MODE_WIREFRAME = 4;
static const uint MODE_CASCADES = 5;
static const uint MODE_LIGHT_COMPLEXITY = 6;

struct PushConstants {
	column_major float4x4 model;
	float4 color;
	uint materialIndex;
	uint mode;
};

[[vk::push_constant]]
PushConstants push;

struct VSInput {
	[[vk::location(0)]]
	float3 position : POSITION;
	[[vk::location(1)]]
	float3 color : COLOR;
	[[vk::location(2)]]
	float3 normal : NORMAL;
	[[vk::location(3)]]
	float4 tangent : TANGENT;
	[[vk::location(4)]]
	float2 uv : TEXCOORD0;
};

struct VSOutput {
	float4 sv_position : SV_Position;
	[[vk::location(0)]]
	float3 worldPos : TEXCOORD0;
	[[vk::location(1)]]
	float3 normal : TEXCOORD1;
};

[shader("vertex")]
VSOutput main(VSInput IN) {
	VSOutput OUT;
	float4 worldPos = mul(push.model, float4(IN.position, 1.0));
	OUT.sv_position = mul(GetMainCamera().viewProjection, worldPos);
	OUT.worldPos = worldPos.xyz;
	OUT.normal = normalize(mul((float3x3)push.model, IN.normal));
	return OUT;
}

float3 HeatRamp(float t) {
	t = saturate(t);
	float3 cold = float3(0.12, 0.2, 0.55);
	float3 mid = float3(0.18, 0.68, 0.36);
	float3 warm = float3(0.9, 0.78, 0.22);
	float3 hot = float3(0.9, 0.28, 0.2);
	if (t < 0.33) {
		return lerp(cold, mid, t / 0.33);
	}
	if (t < 0.66) {
		return lerp(mid, warm, (t - 0.33) / 0.33);
	}
	return lerp(warm, hot, (t - 0.66) / 0.34);
}

[shader("fragment")]
float4 main(VSOutput IN) : SV_Target {
	float3 N = normalize(IN.normal);
	float3 V = normalize(GetCameraPosition() - IN.worldPos);
	float facing = 0.55 + 0.45 * saturate(dot(N, V));

	if (push.mode == MODE_ALBEDO) {
		SurfaceData surf = GetSurface(push.materialIndex);
		return float4(surf.albedo.rgb * push.color.rgb, 1.0);
	}
	if (push.mode == MODE_NORMALS) {
		return float4((N * 0.5 + 0.5), 1.0);
	}
	if (push.mode == MODE_DEPTH) {
		float distance = length(GetCameraPosition() - IN.worldPos);
		float t = 1.0 - exp(-distance / 12.0);
		return float4((float3(1.0 - t)), 1.0);
	}
	if (push.mode == MODE_WIREFRAME) {
		return float4(0.7, 0.7, 0.7, 1.0);
	}
	if (push.mode == MODE_CASCADES) {
		float3 L = normalize(-shadowData.lightDirection.xyz);
		float shadow = ComputeDirectionalShadow(IN.worldPos, N, L, IN.sv_position.xy);
		return float4((CascadeDebugColor(IN.worldPos)) * (0.25 + 0.75 * shadow) * facing, 1.0);
	}
	if (push.mode == MODE_LIGHT_COMPLEXITY) {
		float viewZ = mul(GetMainCamera().view, float4(IN.worldPos, 1.0)).z;
		ClusterLightInfo info = GetClusterInfo(GetClusterIndex(IN.sv_position.xy, viewZ));
		float t = saturate(float(info.count) / 8.0);
		return float4((HeatRamp(t)) * facing, 1.0);
	}
	return float4(push.color.rgb * facing, 1.0);
}

#endif
