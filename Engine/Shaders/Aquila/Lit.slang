#ifndef AQUILA_LIT_SLANG
#define AQUILA_LIT_SLANG

import Utils;
import Utility.FrameData;
import Utility.LightData;
import Utility.ClusterData;
import Utility.EnvironmentData;
import Utility.ShadowData;
import Aquila.Surface;

using namespace Aquila::Shading;

struct ShadingPoint {
	float3 N;
	float3 V;
	float3 R;
	float NdotV;
	float3 albedo;
	float metallic;
	float roughness;
	float alpha;
	float3 F0;
	float3 energyCompensation;
};

float D_GGX(float NdotH, float alpha) {
	float a2 = alpha * alpha;
	float d = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
	return a2 / (PI * d * d + EPSILON);
}

float V_SmithGGXCorrelated(float NdotV, float NdotL, float alpha) {
	float a2 = alpha * alpha;
	float lambdaV = NdotL * sqrt(NdotV * NdotV * (1.0 - a2) + a2);
	float lambdaL = NdotV * sqrt(NdotL * NdotL * (1.0 - a2) + a2);
	return 0.5 / max(lambdaV + lambdaL, 1e-5);
}

float3 FresnelSchlick(float cosTheta, float3 F0) {
	return F0 + (1.0 - F0) * pow(saturate(1.0 - cosTheta), 5.0);
}

float2 EnvBRDFApprox(float roughness, float NdotV) {
	const float4 c0 = float4(-1.0, -0.0275, -0.572, 0.022);
	const float4 c1 = float4(1.0, 0.0425, 1.04, -0.04);
	float4 r = roughness * c0 + c1;
	float a004 = min(r.x * r.x, exp2(-9.28 * NdotV)) * r.x + r.y;
	return float2(-1.04, 1.04) * a004 + r.zw;
}

float3 EvaluateLight(ShadingPoint p, float3 diffuseL, float3 specularL, float specularScale, float3 radiance) {
	float NdotLd = saturate(dot(p.N, diffuseL));
	float NdotLs = saturate(dot(p.N, specularL));
	if (NdotLd <= 0.0 && NdotLs <= 0.0) {
		return float3(0.0);
	}

	float3 H = normalize(p.V + specularL);
	float NdotH = saturate(dot(p.N, H));
	float VdotH = saturate(dot(p.V, H));
	float3 F = FresnelSchlick(VdotH, p.F0);
	float D = D_GGX(NdotH, p.alpha) * specularScale;
	float Vis = V_SmithGGXCorrelated(p.NdotV, NdotLs, p.alpha);

	float3 specular = D * Vis * F * p.energyCompensation * NdotLs;
	float3 diffuse = (1.0 - F) * (1.0 - p.metallic) * p.albedo * INV_PI * NdotLd;
	return (diffuse + specular) * radiance;
}

float WidenedLobeScale(float alpha, float angularRadius) {
	float widened = saturate(alpha + angularRadius * 0.5);
	return (alpha * alpha) / max(widened * widened, 1e-8);
}

float3 SunDiskDirection(float3 L, float3 R, float sinRadius, float cosRadius) {
	float LdotR = dot(L, R);
	if (LdotR >= cosRadius) {
		return R;
	}
	float3 toward = R - LdotR * L;
	float len = length(toward);
	if (len < 1e-5) {
		return L;
	}
	return normalize(L * cosRadius + (toward / len) * sinRadius);
}

float3 SphereClosestDirection(float3 toLight, float3 R, float radius) {
	float3 centerToRay = dot(toLight, R) * R - toLight;
	float3 closest = toLight + centerToRay * saturate(radius / max(length(centerToRay), 1e-5));
	return normalize(closest);
}

float3 ShadeSurface(Surface s, SurfaceInput input, bool receivesShadows) {
	ShadingPoint p;
	p.N = normalize(s.normal);
	p.V = input.viewDirection;
	p.NdotV = max(dot(p.N, p.V), 1e-4);
	p.R = reflect(-p.V, p.N);
	p.albedo = s.baseColor;
	p.metallic = saturate(s.metallic);
	p.roughness = clamp(s.roughness, 0.045, 1.0);
	p.alpha = p.roughness * p.roughness;
	p.F0 = lerp(float3(0.04), p.albedo, p.metallic);
	float2 envBRDF = EnvBRDFApprox(p.roughness, p.NdotV);
	p.energyCompensation = 1.0 + p.F0 * (1.0 / max(envBRDF.x + envBRDF.y, 1e-3) - 1.0);
	float ao = saturate(s.occlusion);

	float viewZ = mul(GetMainCamera().view, float4(input.worldPosition, 1.0)).z;
	ClusterLightInfo clusterInfo = GetClusterInfo(GetClusterIndex(input.pixel, viewZ));

	float3 Lo = float3(0.0);
	for (uint i = 0; i < clusterInfo.count; ++i) {
		LightData light = GetLight(GetLightListIndex(clusterInfo.offset + i));
		float3 lightColor = GetLightColor(light) * GetLightIntensity(light);

		if (IsDirectionalLight(light)) {
			float3 L = normalize(-GetLightDirection(light));
			float sinRadius = GetSunSinRadius(light);
			float3 specularL = L;
			float specularScale = 1.0;
			if (sinRadius > 0.0) {
				specularL = SunDiskDirection(L, p.R, sinRadius, GetSunCosRadius(light));
				specularScale = WidenedLobeScale(p.alpha, sinRadius);
			}
			float shadow = 1.0;
			if (receivesShadows && light.shadowIndex >= 0) {
				shadow = ComputeDirectionalShadow(input.worldPosition, p.N, L, input.pixel);
			}
			Lo += EvaluateLight(p, L, specularL, specularScale, lightColor) * shadow;
		} else {
			float3 toLight = GetLightPosition(light) - input.worldPosition;
			float dist = max(length(toLight), 1e-4);
			float3 L = toLight / dist;
			float radius = GetSourceRadius(light);
			float3 specularL = L;
			float specularScale = 1.0;
			if (radius > 0.0) {
				specularL = SphereClosestDirection(toLight, p.R, radius);
				specularScale = WidenedLobeScale(p.alpha, radius / dist);
			}

			float atten = SphereAttenuation(dist, GetLightRange(light), radius);
			if (IsSpotLight(light)) {
				atten *= SpotAttenuation(L, light);
			}
			if (atten > 0.0 && receivesShadows && light.shadowIndex >= 0) {
				atten *= ComputeLocalShadow(light.shadowIndex, IsPointLight(light), GetLightPosition(light),
											input.worldPosition, p.N, L, input.pixel);
			}

			Lo += EvaluateLight(p, L, specularL, specularScale, lightColor * atten);
		}
	}

	float3 ambient = float3(0.0);
	if (IsEnvironmentEnabled()) {
		float3 specularAlbedo = p.F0 * envBRDF.x + envBRDF.y;
		float3 kD = (1.0 - specularAlbedo) * (1.0 - p.metallic);
		float specularOcclusion = saturate(pow(p.NdotV + ao, exp2(-16.0 * p.roughness - 1.0)) - 1.0 + ao);
		float3 diffuseAmbient = kD * p.albedo * EvaluateSH(p.N) * ao;
		float3 specularAmbient = EvaluateSH(p.R) * specularAlbedo * p.energyCompensation * specularOcclusion;
		ambient = diffuseAmbient + specularAmbient;
	}

	return ApplyFog(Lo + ambient + s.emissive, GetCameraPosition(), input.worldPosition);
}

#endif 