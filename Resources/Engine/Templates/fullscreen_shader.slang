import Aquila.Fullscreen;

[[vk::binding(0, 1)]]
cbuffer Effect {
	float4 tintColor;
	float vignetteStrength;
};

float4 fullscreen(FullscreenInput input) {
	float4 scene = SampleScene(input.uv);
	float2 centered = input.uv - 0.5;
	float vignette = saturate(1.0 - vignetteStrength * dot(centered, centered) * 2.0);
	return float4(scene.rgb * tintColor.rgb * vignette, scene.a);
}
