#ifndef AQUILA_EXPOSURE_HISTOGRAM_SLANG
#define AQUILA_EXPOSURE_HISTOGRAM_SLANG

static const uint BIN_COUNT = 256;

[[vk::binding(0, 0)]]
Sampler2D sceneColor;

[[vk::binding(1, 0)]]
RWStructuredBuffer<uint> histogram;

struct HistogramPush {
	uint width;
	uint height;
	uint step;
	float inverseExposure;
	float minLog2;
	float inverseLog2Range;
};

[[vk::push_constant]]
HistogramPush push;

groupshared uint localBins[BIN_COUNT];

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 id : SV_DispatchThreadID, uint index : SV_GroupIndex) {
	localBins[index] = 0;
	GroupMemoryBarrierWithGroupSync();

	uint2 pixel = id.xy * push.step + push.step / 2;
	if (pixel.x < push.width && pixel.y < push.height) {
		float2 uv = (float2(pixel) + 0.5) / float2(push.width, push.height);
		float3 color = sceneColor.SampleLevel(uv, 0.0).rgb;
		float luminance = dot(color, float3(0.2126, 0.7152, 0.0722)) * push.inverseExposure;
		uint bin = 0;
		if (luminance > 1e-8) {
			float t = saturate((log2(luminance) - push.minLog2) * push.inverseLog2Range);
			bin = uint(t * 254.0 + 1.0);
		}
		InterlockedAdd(localBins[bin], 1u);
	}
	GroupMemoryBarrierWithGroupSync();

	if (localBins[index] > 0) {
		InterlockedAdd(histogram[index], localBins[index]);
	}
}

#endif
