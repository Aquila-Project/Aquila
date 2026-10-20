import Aquila.Unlit;

[[vk::binding(0, 1)]]
cbuffer UnlitMaterial {
	float4 color;
	float2 uvScale;
	float2 uvOffset;
};

[[vk::binding(1, 1)]] Sampler2D colorMap;

float4 unlit(SurfaceInput input) {
	return colorMap.Sample(input.uv * uvScale + uvOffset) * color;
}
