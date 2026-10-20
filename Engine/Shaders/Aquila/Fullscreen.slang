#ifndef AQUILA_FULLSCREEN_SLANG
#define AQUILA_FULLSCREEN_SLANG

import Utility.FrameData;

using namespace Aquila::Shading;

[[vk::binding(14, 1)]]
Sampler2D sceneColor;
[[vk::binding(15, 1)]]
Sampler2D sceneDepth;

struct FullscreenInput {
	float2 uv;
	float2 pixel;
};

float4 SampleScene(float2 uv) {
	return sceneColor.SampleLevel(uv, 0.0);
}

float SceneDepth(float2 uv) {
	return sceneDepth.SampleLevel(uv, 0.0).r;
}

float SceneLinearDepth(float2 uv) {
	return LinearizeDepth(SceneDepth(uv));
}

float3 SceneWorldPosition(float2 uv) {
	return ReconstructWorldPos(uv, SceneDepth(uv));
}

#endif
