#ifndef AQUILA_SURFACE_SLANG
#define AQUILA_SURFACE_SLANG

struct SurfaceInput {
	float3 worldPosition;
	float3 normal;
	float4 tangent;
	float2 uv;
	float3 vertexColor;
	float3 viewDirection;
	float2 pixel;
};

struct Surface {
	float3 baseColor;
	float alpha;
	float metallic;
	float roughness;
	float3 normal;
	float3 emissive;
	float occlusion;
};

Surface DefaultSurface(SurfaceInput input) {
	Surface s;
	s.baseColor = float3(1.0);
	s.alpha = 1.0;
	s.metallic = 0.0;
	s.roughness = 0.5;
	s.normal = input.normal;
	s.emissive = float3(0.0);
	s.occlusion = 1.0;
	return s;
}

float3 UnpackNormal(float4 texel, float strength) {
	float3 n = texel.xyz * 2.0 - 1.0;
	n.xy *= strength;
	return normalize(float3(n.xy, max(n.z, 1e-4)));
}

bool TangentBasis(SurfaceInput input, out float3 T, out float3 B, out float3 N) {
	N = normalize(input.normal);
	T = input.tangent.xyz - N * dot(N, input.tangent.xyz);
	B = float3(0.0);
	if (dot(T, T) < 1e-8) {
		return false;
	}
	T = normalize(T);
	B = cross(N, T) * (input.tangent.w < 0.0 ? -1.0 : 1.0);
	return true;
}

float3 PerturbNormal(SurfaceInput input, float3 tangentNormal) {
	float3 T, B, N;
	if (!TangentBasis(input, T, B, N)) {
		return N;
	}
	return normalize(T * tangentNormal.x + B * tangentNormal.y + N * tangentNormal.z);
}

float2 ParallaxOcclusionUV(SurfaceInput input, Sampler2D heightMap, float2 uv, float depth) {
	const float2 dx = ddx(uv);
	const float2 dy = ddy(uv);
	float3 T, B, N;
	if (depth <= 0.0 || !TangentBasis(input, T, B, N)) {
		return uv;
	}
	const float3 V = float3(dot(T, input.viewDirection), dot(B, input.viewDirection), dot(N, input.viewDirection));
	if (V.z <= 0.0) {
		return uv;
	}

	const int maxLayers = 32;
	const float layerStep = 1.0 / lerp(float(maxLayers), 8.0, V.z);
	const float2 uvStep = float2(-V.x, V.y) / max(V.z, 0.1) * depth * layerStep;

	float2 currentUV = uv;
	float layerDepth = 0.0;
	float surfaceDepth = 1.0 - heightMap.SampleGrad(currentUV, dx, dy).r;
	float previousSurfaceDepth = surfaceDepth;
	for (int i = 0; i < maxLayers && layerDepth < surfaceDepth; ++i) {
		currentUV += uvStep;
		layerDepth += layerStep;
		previousSurfaceDepth = surfaceDepth;
		surfaceDepth = 1.0 - heightMap.SampleGrad(currentUV, dx, dy).r;
	}

	const float after = surfaceDepth - layerDepth;
	const float before = previousSurfaceDepth - (layerDepth - layerStep);
	const float denominator = after - before;
	const float weight = abs(denominator) > 1e-6 ? saturate(after / denominator) : 0.0;
	return lerp(currentUV, currentUV - uvStep, weight);
}

#endif
