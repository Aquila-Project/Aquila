import Aquila.Lit;

[[vk::binding(0, 1)]]
cbuffer Material {
	float4 tintColor;
	float3 emissiveColor;
	float emissiveIntensity;
	float roughness;
	float metallic;
	float2 uvScale;
};

[[vk::binding(1, 1)]] Sampler2D albedoMap;
[[vk::binding(2, 1)]] Sampler2D normalMap;

void surface(inout Surface s, SurfaceInput input) {
	float2 uv = input.uv * uvScale;
	float4 albedo = albedoMap.Sample(uv) * tintColor;
	s.baseColor = albedo.rgb;
	s.alpha = albedo.a;
	s.roughness = roughness;
	s.metallic = metallic;
	s.normal = PerturbNormal(input, UnpackNormal(normalMap.Sample(uv), 1.0));
	s.emissive = emissiveColor * emissiveIntensity;
}
