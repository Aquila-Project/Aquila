#ifndef AQUILA_BASIC_SLANG
#define AQUILA_BASIC_SLANG

import Utility.FrameData;
import Utility.LightData;
import Utility.SurfaceData;
import Utility.ClusterData;
import Utility.EnvironmentData;
import Utility.ShadowData;
import "Utils.slang";

using namespace Aquila::Shading;

static const bool kDebugShadowCascades = false;

struct PushConstants {
	column_major float4x4 model;
	float4 color;
	uint materialIndex;
	uint receivesShadows;
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
	[[vk::location(2)]]
	float2 uv : TEXCOORD2;
};

[shader("vertex")]
VSOutput main(VSInput IN) {
	VSOutput OUT;
	float4 worldPos = mul(push.model, float4(IN.position, 1.0));
	OUT.sv_position = mul(GetMainCamera().viewProjection, worldPos);
	OUT.worldPos = worldPos.xyz;
	OUT.normal = normalize(mul((float3x3)push.model, IN.normal));
	OUT.uv = IN.uv;
	return OUT;
}

float DistributionGGX(float NdotH, float roughness) {
	float a = roughness * roughness;
	float a2 = a * a;
	float d = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
	return a2 / (PI * d * d + EPSILON);
}

float GeometrySchlickGGX(float NdotX, float k) {
	return NdotX / (NdotX * (1.0 - k) + k);
}

float GeometrySmith(float NdotV, float NdotL, float roughness) {
	float r = roughness + 1.0;
	float k = (r * r) / 8.0;
	return GeometrySchlickGGX(NdotV, k) * GeometrySchlickGGX(NdotL, k);
}

float3 FresnelSchlick(float cosTheta, float3 F0) {
	return F0 + (1.0 - F0) * pow(saturate(1.0 - cosTheta), 5.0);
}

float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness) {
	float3 Fr = max(float3(1.0 - roughness), F0);
	return F0 + (Fr - F0) * pow(saturate(1.0 - cosTheta), 5.0);
}

float3 SpecularBRDF(float3 N, float3 V, float3 L, float roughness, float3 F0, out float3 F) {
	float3 H = normalize(V + L);
	float NdotV = max(dot(N, V), 1e-4);
	float NdotL = max(dot(N, L), 0.0);
	float NdotH = max(dot(N, H), 0.0);
	float HdotV = max(dot(H, V), 0.0);

	float D = DistributionGGX(NdotH, roughness);
	float G = GeometrySmith(NdotV, NdotL, roughness);
	F = FresnelSchlick(HdotV, F0);

	return (D * G * F) / (4.0 * NdotV * NdotL + EPSILON);
}

float3 ShadeLight(float3 N, float3 V, float3 L, float3 radiance, float3 albedo, float metallic, float roughness,
				  float3 F0) {
	float NdotL = max(dot(N, L), 0.0);
	if (NdotL <= 0.0) {
		return float3(0.0);
	}

	float3 F;
	float3 specular = SpecularBRDF(N, V, L, roughness, F0, F);
	float3 kD = (float3(1.0) - F) * (1.0 - metallic);
	float3 diffuse = kD * albedo * INV_PI;

	return (diffuse + specular) * radiance * NdotL;
}

[shader("fragment")]
float4 main(VSOutput IN) : SV_Target {
	SurfaceData surf = GetSurface(push.materialIndex);
	float3 albedo = surf.albedo.rgb * push.color.rgb;
	float metallic = saturate(surf.metallic);
	float roughness = clamp(surf.roughness, 0.045, 1.0);
	float ao = surf.aoStrength;

	float3 N = normalize(IN.normal);
	float3 V = normalize(GetCameraPosition() - IN.worldPos);
	float NdotV = max(dot(N, V), 1e-4);
	float3 F0 = lerp(float3(0.04), albedo, metallic);

	float viewZ = mul(GetMainCamera().view, float4(IN.worldPos, 1.0)).z;
	uint clusterIdx = GetClusterIndex(IN.sv_position.xy, viewZ);
	ClusterLightInfo clusterInfo = GetClusterInfo(clusterIdx);

	float3 Lo = float3(0.0);

	for (uint i = 0; i < clusterInfo.count; ++i) {
		uint lightIdx = GetLightListIndex(clusterInfo.offset + i);
		LightData light = GetLight(lightIdx);
		float3 lightColor = GetLightColor(light) * GetLightIntensity(light);

		if (IsDirectionalLight(light)) {
			float3 L = normalize(-GetLightDirection(light));
			float shadow = 1.0;
			if (push.receivesShadows != 0 && light.shadowIndex >= 0) {
				shadow = ComputeDirectionalShadow(IN.worldPos, N, L);
			}
			Lo += ShadeLight(N, V, L, lightColor, albedo, metallic, roughness, F0) * shadow;
		} else {
			float3 toLight = GetLightPosition(light) - IN.worldPos;
			float dist = length(toLight);
			float3 L = toLight / dist;
			float atten = Attenuation(dist, GetLightRange(light));

			float spotAtten = 1.0;
			if (IsSpotLight(light)) {
				spotAtten = SpotAttenuation(L, light);
			}

			float3 radiance = lightColor * atten * spotAtten;
			Lo += ShadeLight(N, V, L, radiance, albedo, metallic, roughness, F0);
		}
	}

	float3 ambient = albedo * ao * 0.03;
	if (IsEnvironmentEnabled()) {
		float3 F = FresnelSchlickRoughness(NdotV, F0, roughness);
		float3 kD = (float3(1.0) - F) * (1.0 - metallic);
		float3 irradiance = EvaluateSH(N);
		ambient = kD * albedo * irradiance * ao;
	}

	float3 emissive = surf.emissive.rgb * surf.emissive.a;
	float3 color = Lo + ambient + emissive;

	if (kDebugShadowCascades) {
		float3 Ldir = normalize(-shadowData.lightDirection.xyz);

		float shadow = 1.0;

		if (push.receivesShadows != 0) {
			shadow = ComputeDirectionalShadow(IN.worldPos, N, Ldir);
		}

		return float4(CascadeDebugColor(IN.worldPos) * shadow, 1.0);
	}
	return float4(color * GetExposure(), surf.albedo.a);
}

#endif // AQUILA_BASIC_SLANG
