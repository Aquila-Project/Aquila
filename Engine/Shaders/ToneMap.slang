#ifndef AQUILA_TONE_MAP_SLANG
#define AQUILA_TONE_MAP_SLANG

import "Utils.slang";

static const uint TONE_MAPPER_ACES = 1;
static const uint TONE_MAPPER_AGX = 2;
static const uint TONE_MAPPER_NEUTRAL = 3;

[[vk::binding(0, 0)]]
Sampler2D sceneColor;

struct ToneMapPush {
	uint toneMapper;
	uint displayRaw;
};

[[vk::push_constant]]
ToneMapPush push;

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

float3 AcesFitted(float3 color) {
	const float3x3 inputMatrix =
		float3x3(0.59719, 0.35458, 0.04823, 0.07600, 0.90834, 0.01566, 0.02840, 0.13383, 0.83777);
	const float3x3 outputMatrix =
		float3x3(1.60475, -0.53108, -0.07367, -0.10208, 1.10813, -0.00605, -0.00327, -0.07276, 1.07602);

	color = mul(inputMatrix, color);
	float3 a = color * (color + 0.0245786) - 0.000090537;
	float3 b = color * (0.983729 * color + 0.4329510) + 0.238081;
	return saturate(mul(outputMatrix, a / b));
}

float3 AgxContrast(float3 x) {
	float3 x2 = x * x;
	float3 x4 = x2 * x2;
	return 15.5 * x4 * x2 - 40.14 * x4 * x + 31.96 * x4 - 6.868 * x2 * x + 0.4298 * x2 + 0.1191 * x - 0.00232;
}

float3 Agx(float3 color) {
	const float3x3 inset =
		float3x3(0.842479062253094, 0.0423282422610123, 0.0423756549057051, 0.0784335999999992, 0.878468636469772,
				 0.0784336, 0.0792237451477643, 0.0791661274605434, 0.879142973793104);
	const float3x3 outset =
		float3x3(1.19687900512017, -0.0528968517574562, -0.0529716355144438, -0.0980208811401368, 1.15190312990417,
				 -0.0980434501171241, -0.0990297440797205, -0.0989611768448433, 1.15107367264116);
	const float minEv = -12.47393;
	const float maxEv = 4.026069;

	color = mul(color, inset);
	color = clamp(log2(max(color, float3(1e-10))), minEv, maxEv);
	color = (color - minEv) / (maxEv - minEv);
	color = AgxContrast(color);
	color = mul(color, outset);
	return pow(saturate(color), 2.2);
}

float3 PbrNeutral(float3 color) {
	const float startCompression = 0.8 - 0.04;
	const float desaturation = 0.15;

	float x = min(color.r, min(color.g, color.b));
	float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
	color -= offset;

	float peak = max(color.r, max(color.g, color.b));
	if (peak < startCompression) {
		return color;
	}

	const float d = 1.0 - startCompression;
	float newPeak = 1.0 - d * d / (peak + d - startCompression);
	color *= newPeak / peak;

	float g = 1.0 - 1.0 / (desaturation * (peak - newPeak) + 1.0);
	return lerp(color, float3(newPeak), g);
}

float LinearToSrgb(float c) {
	return c <= 0.0031308 ? c * 12.92 : 1.055 * pow(c, 1.0 / 2.4) - 0.055;
}

float3 LinearToSrgb(float3 c) {
	c = saturate(c);
	return float3(LinearToSrgb(c.r), LinearToSrgb(c.g), LinearToSrgb(c.b));
}

// Pseudo-random generator for dithering based on pixel coordinates
float rand(float2 co) {
	return frac(sin(dot(co, float2(12.9898, 78.233))) * 43758.5453);
}

// Triangular noise dither (prevents visible banding patterns)
float3 TriangularDither(float2 uv, float2 screenSize) {
	float2 pixelCoord = uv * screenSize;
	// Generate two random values
	float r1 = rand(pixelCoord);
	float r2 = rand(pixelCoord + float2(1.414, 3.141));
	// Convert uniform noise to triangular distribution noise [-1, 1]
	float dith = r1 + r2 - 1.0;

	// Scale factor based on 8-bit precision (1.0 / 255.0).
	// Even if outputting to 16F, subtle dither stops fine gradients from stripping.
	return float3(dith) * (1.0 / 255.0);
}

[shader("fragment")]
float4 main(VSOutput input) : SV_Target {
	float4 hdr = sceneColor.Sample(input.uv);
	if (push.displayRaw != 0) {
		return hdr;
	}

	float3 color = max(hdr.rgb, float3(0.0));
	if (push.toneMapper == TONE_MAPPER_ACES) {
		color = AcesFitted(color);
	} else if (push.toneMapper == TONE_MAPPER_AGX) {
		color = Agx(color);
	} else if (push.toneMapper == TONE_MAPPER_NEUTRAL) {
		color = PbrNeutral(color);
	}

	color = LinearToSrgb(color);

	float2 screenSize;
	sceneColor.GetDimensions(screenSize.x, screenSize.y);
	color += TriangularDither(input.uv, screenSize);

	return float4(saturate(color), saturate(hdr.a));
}

#endif 