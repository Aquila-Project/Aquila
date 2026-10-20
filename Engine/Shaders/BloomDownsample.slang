#ifndef AQUILA_BLOOM_DOWNSAMPLE_SLANG
#define AQUILA_BLOOM_DOWNSAMPLE_SLANG

import "Utils.slang";

[[vk::binding(0, 0)]]
Sampler2D sceneColor;

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
	sceneColor.GetDimensions(srcResolution.x, srcResolution.y);
	float2 srcTexelSize = 1.0 / srcResolution;
	float x = srcTexelSize.x;
	float y = srcTexelSize.y;

	float3 downsample;

	// Take 13 samples around current texel:
	// a - b - c
	// - j - k -
	// d - e - f
	// - l - m -
	// g - h - i
	// === ('e' is the current texel) ===
	float3 a = sceneColor.Sample(float2(input.uv.x - 2 * x, input.uv.y + 2 * y)).rgb;
	float3 b = sceneColor.Sample(float2(input.uv.x, input.uv.y + 2 * y)).rgb;
	float3 c = sceneColor.Sample(float2(input.uv.x + 2 * x, input.uv.y + 2 * y)).rgb;

	float3 d = sceneColor.Sample(float2(input.uv.x - 2 * x, input.uv.y)).rgb;
	float3 e = sceneColor.Sample(float2(input.uv.x, input.uv.y)).rgb;
	float3 f = sceneColor.Sample(float2(input.uv.x + 2 * x, input.uv.y)).rgb;

	float3 g = sceneColor.Sample(float2(input.uv.x - 2 * x, input.uv.y - 2 * y)).rgb;
	float3 h = sceneColor.Sample(float2(input.uv.x, input.uv.y - 2 * y)).rgb;
	float3 i = sceneColor.Sample(float2(input.uv.x + 2 * x, input.uv.y - 2 * y)).rgb;

	float3 j = sceneColor.Sample(float2(input.uv.x - x, input.uv.y + y)).rgb;
	float3 k = sceneColor.Sample(float2(input.uv.x + x, input.uv.y + y)).rgb;
	float3 l = sceneColor.Sample(float2(input.uv.x - x, input.uv.y - y)).rgb;
	float3 m = sceneColor.Sample(float2(input.uv.x + x, input.uv.y - y)).rgb;

	downsample = e * 0.125;
	downsample += (a + c + g + i) * 0.03125;
	downsample += (b + d + f + h) * 0.0625;
	downsample += (j + k + l + m) * 0.125;

	return float4(downsample, 1.0);
}
#endif 