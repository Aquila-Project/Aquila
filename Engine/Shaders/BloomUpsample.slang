#ifndef AQUILA_BLOOM_UPSAMPLE_SLANG
#define AQUILA_BLOOM_UPSAMPLE_SLANG

import "Utils.slang";

[[vk::binding(0, 0)]]
Sampler2D downsampleInput;

struct VSOutput {
	float4 position : SV_Position;
	float2 uv : TEXCOORD0;
};

[shader("vertex")]
VSOutput main(uint vertexIndex: SV_VertexID) {
	VSOutput o;
	o.position = FullscreenVertexPosition(vertexIndex);
	o.uv = FullscreenTexCoord(vertexIndex);
	return o;
}

[shader("fragment")]
float4 main(VSOutput input) : SV_Target {
	float2 srcResolution;
	downsampleInput.GetDimensions(srcResolution.x, srcResolution.y);

	float filterRadius = 1;

	float x = filterRadius / srcResolution.x;
	float y = filterRadius / srcResolution.y;

	float3 upsample;

	// Take 13 samples around current texel:
	// a - b - c
	// - j - k -
	// d - e - f
	// - l - m -
	// g - h - i
	// === ('e' is the current texel) ===
	float3 a = downsampleInput.Sample(float2(input.uv.x - x, input.uv.y + y)).rgb;
	float3 b = downsampleInput.Sample(float2(input.uv.x, input.uv.y + y)).rgb;
	float3 c = downsampleInput.Sample(float2(input.uv.x + x, input.uv.y + y)).rgb;

	float3 d = downsampleInput.Sample(float2(input.uv.x - x, input.uv.y)).rgb;
	float3 e = downsampleInput.Sample(float2(input.uv.x, input.uv.y)).rgb;
	float3 f = downsampleInput.Sample(float2(input.uv.x + x, input.uv.y)).rgb;

	float3 g = downsampleInput.Sample(float2(input.uv.x - x, input.uv.y - y)).rgb;
	float3 h = downsampleInput.Sample(float2(input.uv.x, input.uv.y - y)).rgb;
	float3 i = downsampleInput.Sample(float2(input.uv.x + x, input.uv.y - y)).rgb;

	upsample = e * 4.0;
	upsample += (b + d + f + h) * 2.0;
	upsample += (a + c + g + i);
	upsample *= 1.0 / 16.0;

	return float4(upsample, 1.0);
}
#endif 