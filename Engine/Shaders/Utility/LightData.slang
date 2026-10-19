#ifndef AQUILA_LIGHT_DATA_SLANG
#define AQUILA_LIGHT_DATA_SLANG

import FrameData;

namespace Aquila::Shading {

static const int LIGHT_POINT = 0;
static const int LIGHT_DIRECTIONAL = 1;
static const int LIGHT_SPOT = 2;
static const int LIGHT_AREA = 3;

struct LightData {
	float4 positionAndRange;  // xyz = world position, w = range
	float4 colorAndIntensity; // xyz = linear color, w = intensity
	float4 directionAndType;  // xyz = direction, w = LIGHT_* type (float-cast)
	float4 rightAndWidth;	  // xyz = right vector (area), w = width  (area)
	float4 upAndHeight;		  // xyz = up vector (area), w = height (area)
	float cosInnerAngle;	  // spot only, 1.0 for other types
	float cosOuterAngle;	  // spot only, 1.0 for other types
	int shadowIndex;		  // -1 = unshadowed
	uint flags;
	float4 source;
};

[vk::binding(1, 0)] StructuredBuffer<LightData> lights;

uint GetLightCount() {
	return frameData.lightCount;
}
LightData GetLight(uint i) {
	return lights[i];
}

int GetLightType(LightData l) {
	return int(l.directionAndType.w);
}
bool IsPointLight(LightData l) {
	return GetLightType(l) == LIGHT_POINT;
}
bool IsDirectionalLight(LightData l) {
	return GetLightType(l) == LIGHT_DIRECTIONAL;
}
bool IsSpotLight(LightData l) {
	return GetLightType(l) == LIGHT_SPOT;
}
bool IsAreaLight(LightData l) {
	return GetLightType(l) == LIGHT_AREA;
}

float3 GetLightPosition(LightData l) {
	return l.positionAndRange.xyz;
}
float GetLightRange(LightData l) {
	return l.positionAndRange.w;
}
float3 GetLightColor(LightData l) {
	return l.colorAndIntensity.xyz;
}
float GetLightIntensity(LightData l) {
	return l.colorAndIntensity.w;
}
float3 GetLightDirection(LightData l) {
	return l.directionAndType.xyz;
}
float GetCosInnerAngle(LightData l) {
	return l.cosInnerAngle;
}
float GetCosOuterAngle(LightData l) {
	return l.cosOuterAngle;
}
float GetSourceRadius(LightData l) {
	return l.source.x;
}
float GetSunSinRadius(LightData l) {
	return l.source.x;
}
float GetSunCosRadius(LightData l) {
	return l.source.y;
}
float3 GetAreaRight(LightData l) {
	return l.rightAndWidth.xyz;
}
float GetAreaWidth(LightData l) {
	return l.rightAndWidth.w;
}
float3 GetAreaUp(LightData l) {
	return l.upAndHeight.xyz;
}
float GetAreaHeight(LightData l) {
	return l.upAndHeight.w;
}

float Attenuation(float dist, float range) {
	float num = saturate(1.0 - pow(dist / range, 4.0));
	return (num * num) / max(dist * dist, 0.0001);
}

float SphereAttenuation(float dist, float range, float radius) {
	float num = saturate(1.0 - pow(dist / range, 4.0));
	return (num * num) / max(dist * dist, max(radius * radius, 0.0001));
}

float SpotAttenuation(float3 L, LightData l) {
	float cosAngle = dot(-L, GetLightDirection(l));
	float t = saturate((cosAngle - l.cosOuterAngle) / max(l.cosInnerAngle - l.cosOuterAngle, 0.0001));
	return t * t;
}

} // namespace Aquila::Shading
#endif // AQUILA_LIGHT_DATA_SLANG
