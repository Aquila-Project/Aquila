import Aquila.Unlit;

[[vk::binding(0, 1)]]
cbuffer Material {
	float4 color;
	float stripeCount;
	float speed;
};

float4 unlit(SurfaceInput input) {
	float stripes = 0.5 + 0.5 * sin(input.uv.x * stripeCount * 6.2831853 + GetTime() * speed);
	return float4(color.rgb * stripes, color.a);
}
