#ifndef AQUILA_ENVIRONMENT_DATA_SLANG
#define AQUILA_ENVIRONMENT_DATA_SLANG

namespace Aquila::Shading {

struct EnvironmentData {
	float4 shCoeffs[9];		 // spherical harmonic coefficients (RGB in xyz, w unused)
	float4 tintAndIntensity; // xyz = tint, w = intensity
	float4 fogColorDensity;
	float4 fogParams;
	float4 fogSunDirection;
	float4 fogSunIlluminance;
	int enabled;
	int3 _pad;
};

[vk::binding(2, 0)] ConstantBuffer<EnvironmentData> environment;

bool IsEnvironmentEnabled() {
	return environment.enabled != 0;
}
float3 GetEnvironmentTint() {
	return environment.tintAndIntensity.xyz;
}
float GetEnvironmentIntensity() {
	return environment.tintAndIntensity.w;
}
float4 GetSHCoeff(uint index) {
	return environment.shCoeffs[index];
}

// Evaluate irradiance from 9-coefficient (L0+L1+L2) SH for surface normal N.
float3 EvaluateSH(float3 N) {
	const float c0 = 0.282095;
	const float c1 = 0.488603;
	const float c2 = 1.092548;
	const float c3 = 0.315392;
	const float c4 = 0.546274;

	float3 result = c0 * environment.shCoeffs[0].xyz - c1 * N.y * environment.shCoeffs[1].xyz +
					c1 * N.z * environment.shCoeffs[2].xyz - c1 * N.x * environment.shCoeffs[3].xyz +
					c2 * N.x * N.y * environment.shCoeffs[4].xyz - c2 * N.y * N.z * environment.shCoeffs[5].xyz +
					c3 * (3.0 * N.z * N.z - 1.0) * environment.shCoeffs[6].xyz -
					c2 * N.x * N.z * environment.shCoeffs[7].xyz +
					c4 * (N.x * N.x - N.y * N.y) * environment.shCoeffs[8].xyz;

	return max(result, float3(0.0)) * environment.tintAndIntensity.w * environment.tintAndIntensity.xyz;
}

bool IsFogEnabled() {
	return environment.fogSunIlluminance.w > 0.0;
}

float FogOpticalDepth(float3 cameraPos, float3 worldPos) {
	float3 ray = worldPos - cameraPos;
	float dist = length(ray);
	float start = environment.fogParams.z;
	if (dist <= start) {
		return 0.0;
	}

	float3 dir = ray / dist;
	float rayLength = dist - start;
	float falloff = max(environment.fogParams.x, 1e-4);
	float startHeight = cameraPos.y + dir.y * start - environment.fogParams.y;
	float densityAtStart = environment.fogColorDensity.w * exp(clamp(-falloff * startHeight, -80.0, 80.0));

	float k = falloff * dir.y * rayLength;
	float integral = 1.0;
	if (k > 1e-4) {
		integral = (1.0 - exp(-min(k, 80.0))) / k;
	} else if (k < -1e-4) {
		k = max(k, -80.0);
		integral = (1.0 - exp(-k)) / k;
	}
	return densityAtStart * rayLength * integral;
}

float3 FogInscattering(float3 viewDir) {
	const float INV_4PI = 0.0795774715;
	float3 ambient = float3(0.0);
	if (IsEnvironmentEnabled()) {
		ambient = environment.shCoeffs[0].xyz * 0.282095 * environment.tintAndIntensity.w *
				  environment.tintAndIntensity.xyz;
	}
	float g = environment.fogSunDirection.w;
	float cosTheta = dot(viewDir, environment.fogSunDirection.xyz);
	float denom = max(1.0 + g * g - 2.0 * g * cosTheta, 1e-4);
	float phase = (1.0 - g * g) * INV_4PI / (denom * sqrt(denom));
	return environment.fogColorDensity.rgb * (max(ambient, float3(0.0)) + environment.fogSunIlluminance.rgb * phase);
}

float3 ApplyFog(float3 radiance, float3 cameraPos, float3 worldPos) {
	if (!IsFogEnabled()) {
		return radiance;
	}
	float3 ray = worldPos - cameraPos;
	float dist = length(ray);
	if (dist < 1e-4) {
		return radiance;
	}
	float amount = min(1.0 - exp(-FogOpticalDepth(cameraPos, worldPos)), environment.fogParams.w);
	return lerp(radiance, FogInscattering(ray / dist), amount);
}

} // namespace Aquila::Shading
#endif // AQUILA_ENVIRONMENT_DATA_SLANG
