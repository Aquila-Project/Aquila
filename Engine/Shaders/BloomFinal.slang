#ifndef AQUILA_BLOOM_FINAL_SLANG
#define AQUILA_BLOOM_FINAL_SLANG

import "Utils.slang";

[[vk::binding(0, 0)]]
Sampler2D sceneColor;

[[vk::binding(1, 0)]]
Sampler2D bloomColor;

struct BloomFinalPush {
	float sceneWeight;
	float bloomWeight;
};

[[vk::push_constant]]
BloomFinalPush push;

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
	float4 scene = sceneColor.Sample(input.uv);
	float3 bloom = bloomColor.Sample(input.uv).rgb;
	return float4(scene.rgb * push.sceneWeight + bloom * push.bloomWeight, scene.a);
}

#endif
