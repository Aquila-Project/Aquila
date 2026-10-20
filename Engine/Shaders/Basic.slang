import Aquila.Lit;

[[vk::binding(0, 1)]]
cbuffer BasicMaterial {
	float4 baseColor;
	float3 emissiveColor;
	float emissiveIntensity;
	float metallic;
	float roughness;
	float normalStrength;
	float occlusionStrength;
	float heightDepth;
	float2 uvScale;
	float2 uvOffset;
};

[[vk::binding(1, 1)]]
Sampler2D baseColorMap;
[[vk::binding(2, 1)]]
Sampler2D normalMap;
[[vk::binding(3, 1)]]
Sampler2D metallicRoughnessMap;
[[vk::binding(4, 1)]]
Sampler2D occlusionMap;
[[vk::binding(5, 1)]]
Sampler2D emissiveMap;
[[vk::binding(6, 1)]]
Sampler2D heightMap;

void surface(inout Surface s, SurfaceInput input) {
	float2 baseUV = input.uv * uvScale + uvOffset;

	float2 uv = ParallaxOcclusionUV(input, heightMap, baseUV, heightDepth);

	float4 base = baseColorMap.Sample(uv) * baseColor;
	float4 metallicRoughness = metallicRoughnessMap.Sample(uv);

	s.baseColor = base.rgb;
	s.alpha = base.a;
	s.metallic = metallic * metallicRoughness.b;
	s.roughness = roughness * metallicRoughness.g;
	s.normal = PerturbNormal(input, UnpackNormal(normalMap.Sample(uv), normalStrength));
	s.occlusion = lerp(1.0, occlusionMap.Sample(uv).r, occlusionStrength);
	s.emissive = emissiveMap.Sample(uv).rgb * emissiveColor * emissiveIntensity;
}
