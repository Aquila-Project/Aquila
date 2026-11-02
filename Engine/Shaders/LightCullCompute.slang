#ifndef AQUILA_LIGHT_CULL_COMPUTE_SLANG
#define AQUILA_LIGHT_CULL_COMPUTE_SLANG

import Utility.FrameData;
import Utility.LightData;
import Utility.ClusterTypes;

using namespace Aquila::Shading;

[vk::binding(0, 1)]
StructuredBuffer<AABB> clusterAABBs;
[vk::binding(1, 1)]
RWStructuredBuffer<uint> lightIndexList;
[vk::binding(2, 1)]
RWStructuredBuffer<ClusterLightInfo> clusterLightInfo;
[vk::binding(3, 1)]
RWStructuredBuffer<uint> globalCounter;

bool SphereIntersectsAABB(float3 center, float radius, float3 aabbMin, float3 aabbMax) {
	float3 closest = clamp(center, aabbMin, aabbMax);
	float3 d = center - closest;
	return dot(d, d) <= radius * radius;
}

[numthreads(4, 4, 4)]
void main(uint3 id: SV_DispatchThreadID) {
	if (id.x >= kClusterGridX || id.y >= kClusterGridY || id.z >= kClusterGridZ) {
		return;
	}

	uint clusterIdx = id.z * (kClusterGridX * kClusterGridY) + id.y * kClusterGridX + id.x;
	AABB aabb = clusterAABBs[clusterIdx];

	float4x4 view = frameData.mainCamera.view;
	uint lightCount = GetLightCount();

	uint localIndices[kMaxLightsPerCluster];
	uint localCount = 0;

	for (uint i = 0; i < lightCount && localCount < kMaxLightsPerCluster; ++i) {
		LightData light = GetLight(i);

		// Directional lights have no position and affect every cluster.
		if (IsDirectionalLight(light)) {
			localIndices[localCount++] = i;
			continue;
		}

		float3 posView = mul(view, float4(GetLightPosition(light), 1.0)).xyz;
		float range = GetLightRange(light);
		if (SphereIntersectsAABB(posView, range, aabb.min, aabb.max)) {
			localIndices[localCount++] = i;
		}
	}

	uint globalOffset = 0;
	InterlockedAdd(globalCounter[0], localCount, globalOffset);

	for (uint j = 0; j < localCount; ++j) {
		lightIndexList[globalOffset + j] = localIndices[j];
	}

	ClusterLightInfo info;
	info.offset = globalOffset;
	info.count = localCount;
	clusterLightInfo[clusterIdx] = info;
}

#endif // AQUILA_LIGHT_CULL_COMPUTE_SLANG
