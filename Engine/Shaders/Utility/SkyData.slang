#ifndef AQUILA_SKY_DATA_SLANG
#define AQUILA_SKY_DATA_SLANG

namespace Aquila::Shading {

struct SkyData {
	float4 configA[3];
	float4 configB[3];
	float4 configC[3];
	float4 sunDirection;
	float4 tintIntensity;
	float4 groundAlbedo;
};

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

float3 SkyRadiance(SkyData data, float3 dir) {
	float3 sunDir = normalize(data.sunDirection.xyz);

	float theta = acos(clamp(dir.y, 0.0, 1.0));
	float gamma = acos(clamp(dot(dir, sunDir), -1.0, 1.0));

	float3 color;
	color.r = HosekRadiance(data.configA[0], data.configB[0], data.configC[0].x, theta, gamma) * data.configC[0].y;
	color.g = HosekRadiance(data.configA[1], data.configB[1], data.configC[1].x, theta, gamma) * data.configC[1].y;
	color.b = HosekRadiance(data.configA[2], data.configB[2], data.configC[2].x, theta, gamma) * data.configC[2].y;
	color = max(color, float3(0.0));

	float3 groundColor = color * data.groundAlbedo.xyz;
	float horizonBlend = smoothstep(-0.02, 0.02, dir.y);
	return lerp(groundColor, color, horizonBlend);
}

}

#endif
