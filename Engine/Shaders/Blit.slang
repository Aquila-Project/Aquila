#ifndef AQUILA_BLIT_SLANG
#define AQUILA_BLIT_SLANG

import "Utils.slang";

[[vk::binding(0, 0)]]
Sampler2D inputTexture;

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
	return inputTexture.Sample(input.uv);
}

#endif // AQUILA_BLIT_SLANG
