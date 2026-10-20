struct AquilaDepthPush {
	column_major float4x4 model;
	column_major float4x4 viewProjection;
	float alphaCutoff;
};

[[vk::push_constant]]
AquilaDepthPush aquilaPush;

struct AquilaVertexInput {
	[[vk::location(0)]] float3 position : POSITION;
	[[vk::location(1)]] float3 color : COLOR;
	[[vk::location(2)]] float3 normal : NORMAL;
	[[vk::location(3)]] float4 tangent : TANGENT;
	[[vk::location(4)]] float2 uv : TEXCOORD0;
};

struct AquilaDepthVertex {
	float4 sv_position : SV_Position;
	[[vk::location(0)]] float3 worldPosition : TEXCOORD0;
	[[vk::location(1)]] float3 normal : TEXCOORD1;
	[[vk::location(2)]] float4 tangent : TEXCOORD2;
	[[vk::location(3)]] float2 uv : TEXCOORD3;
	[[vk::location(4)]] float3 color : TEXCOORD4;
};

[shader("vertex")]
AquilaDepthVertex vertexMain(AquilaVertexInput vertexInput) {
	AquilaDepthVertex vertexOutput;
	float4 world = mul(aquilaPush.model, float4(vertexInput.position, 1.0));
	float3x3 model3 = (float3x3)aquilaPush.model;
	vertexOutput.sv_position = mul(aquilaPush.viewProjection, world);
	vertexOutput.worldPosition = world.xyz;
	vertexOutput.normal = mul(model3, vertexInput.normal);
	vertexOutput.tangent = float4(mul(model3, vertexInput.tangent.xyz), vertexInput.tangent.w);
	vertexOutput.uv = vertexInput.uv;
	vertexOutput.color = vertexInput.color;
	return vertexOutput;
}

[shader("fragment")]
void fragmentMain(AquilaDepthVertex fragmentInput) {
	SurfaceInput surfaceInput;
	surfaceInput.worldPosition = fragmentInput.worldPosition;
	surfaceInput.normal = normalize(fragmentInput.normal);
	surfaceInput.tangent = fragmentInput.tangent;
	surfaceInput.uv = fragmentInput.uv;
	surfaceInput.vertexColor = fragmentInput.color;
	surfaceInput.viewDirection = normalize(GetCameraPosition() - fragmentInput.worldPosition);
	surfaceInput.pixel = fragmentInput.sv_position.xy;

	Surface surfaceResult = DefaultSurface(surfaceInput);
	surface(surfaceResult, surfaceInput);
	if (surfaceResult.alpha < aquilaPush.alphaCutoff) {
		discard;
	}
}
