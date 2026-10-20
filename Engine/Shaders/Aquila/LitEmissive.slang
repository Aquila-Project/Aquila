struct AquilaMeshPush {
	column_major float4x4 model;
	float4 color;
	uint materialIndex;
	uint receivesShadows;
	float alphaCutoff;
	uint transparent;
};

[[vk::push_constant]]
AquilaMeshPush aquilaPush;

struct AquilaVertexInput {
	[[vk::location(0)]]
	float3 position : POSITION;
	[[vk::location(1)]]
	float3 color : COLOR;
	[[vk::location(2)]]
	float3 normal : NORMAL;
	[[vk::location(3)]]
	float4 tangent : TANGENT;
	[[vk::location(4)]]
	float2 uv : TEXCOORD0;
};

struct AquilaVertexOutput {
	float4 sv_position : SV_Position;
	[[vk::location(0)]]
	float3 worldPosition : TEXCOORD0;
	[[vk::location(1)]]
	float3 normal : TEXCOORD1;
	[[vk::location(2)]]
	float4 tangent : TEXCOORD2;
	[[vk::location(3)]]
	float2 uv : TEXCOORD3;
	[[vk::location(4)]]
	float3 color : TEXCOORD4;
};

[shader("vertex")]
AquilaVertexOutput vertexMain(AquilaVertexInput vertexInput) {
	AquilaVertexOutput vertexOutput;
	float4 world = mul(aquilaPush.model, float4(vertexInput.position, 1.0));
	float3x3 model3 = (float3x3)aquilaPush.model;
	vertexOutput.sv_position = mul(GetMainCamera().viewProjection, world);
	vertexOutput.worldPosition = world.xyz;
	vertexOutput.normal = mul(model3, vertexInput.normal);
	vertexOutput.tangent = float4(mul(model3, vertexInput.tangent.xyz), vertexInput.tangent.w);
	vertexOutput.uv = vertexInput.uv;
	vertexOutput.color = vertexInput.color;
	return vertexOutput;
}

[shader("fragment")]
float4 fragmentMain(AquilaVertexOutput fragmentInput) : SV_Target {
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

	return float4(max(surfaceResult.emissive, float3(0.0)) * GetExposure(), 1.0);
}
