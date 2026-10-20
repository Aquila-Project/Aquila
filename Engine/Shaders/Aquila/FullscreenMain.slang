struct AquilaFullscreenVertex {
	float4 position : SV_Position;
	[[vk::location(0)]] float2 uv : TEXCOORD0;
};

[shader("vertex")]
AquilaFullscreenVertex vertexMain(uint vertexIndex : SV_VertexID) {
	AquilaFullscreenVertex vertexOutput;
	vertexOutput.position = FullscreenVertexPosition(vertexIndex);
	vertexOutput.uv = FullscreenTexCoord(vertexIndex);
	return vertexOutput;
}

[shader("fragment")]
float4 fragmentMain(AquilaFullscreenVertex fragmentInput) : SV_Target {
	FullscreenInput fullscreenInput;
	fullscreenInput.uv = fragmentInput.uv;
	fullscreenInput.pixel = fragmentInput.position.xy;
	return fullscreen(fullscreenInput);
}
