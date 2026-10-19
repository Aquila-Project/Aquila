#ifndef AQUILA_FRAME_DATA_SLANG
#define AQUILA_FRAME_DATA_SLANG

namespace Aquila::Shading {

static const uint MAX_CAMERAS = 8;

struct CameraData {
	column_major float4x4 view;
	column_major float4x4 projection;
	column_major float4x4 viewProjection;
	column_major float4x4 inverseView;
	column_major float4x4 inverseProjection;
	column_major float4x4 inverseViewProjection;
	column_major float4x4 prevViewProjection;

	float4 position; // .xyz = world-space position
	float4 forward;	 // .xyz = world-space forward
	float4 up;		 // .xyz = world-space up
	float4 right;	 // .xyz = world-space right

	float nearPlane;
	float farPlane;
	float fov; // vertical FOV in degrees
	float aspectRatio;

	float2 resolution; // viewport size in pixels
	float2 jitter;	   // TAA sub-pixel jitter

	uint isOrthographic;
	uint cameraIndex;
	uint2 _pad;
};

struct FrameData {
	CameraData mainCamera;
	CameraData cameras[MAX_CAMERAS];

	uint cameraCount;
	float time;
	float deltaTime;
	uint frameIndex;

	float2 screenResolution;
	uint lightCount;
	float exposure;
};

[vk::binding(0, 0)] ConstantBuffer<FrameData> frameData;

CameraData GetMainCamera() {
	return frameData.mainCamera;
}
CameraData GetCamera(uint index) {
	return frameData.cameras[index];
}
uint GetCameraCount() {
	return frameData.cameraCount;
}

float3 GetCameraPosition() {
	return frameData.mainCamera.position.xyz;
}
float3 GetCameraForward() {
	return frameData.mainCamera.forward.xyz;
}
float3 GetCameraUp() {
	return frameData.mainCamera.up.xyz;
}
float3 GetCameraRight() {
	return frameData.mainCamera.right.xyz;
}
float GetNearPlane() {
	return frameData.mainCamera.nearPlane;
}
float GetFarPlane() {
	return frameData.mainCamera.farPlane;
}
float GetFOV() {
	return frameData.mainCamera.fov;
}
float GetAspectRatio() {
	return frameData.mainCamera.aspectRatio;
}
bool IsOrthographic() {
	return frameData.mainCamera.isOrthographic != 0;
}

float4 WorldToClip(float3 worldPos) {
	return mul(frameData.mainCamera.viewProjection, float4(worldPos, 1.0));
}

float3 NDCToView(float4x4 matrix, float2 ndc, float linearDepth) {
	float4 clip = float4(ndc, 1.0, 1.0);
	float4 view = mul(matrix, clip);
	view /= view.w;
	return view.xyz * (linearDepth / view.z);
}

float4 WorldToView(float3 worldPos) {
	return mul(frameData.mainCamera.view, float4(worldPos, 1.0));
}

float3 ClipToWorld(float4 clipPos) {
	float4 worldPos = mul(frameData.mainCamera.inverseViewProjection, clipPos);
	return worldPos.xyz / worldPos.w;
}

float3 ViewToWorld(float3 viewPos) {
	return mul(frameData.mainCamera.inverseView, float4(viewPos, 1.0)).xyz;
}

float3 ReconstructWorldPos(float2 uv, float depth) {
	float4 clipPos = float4(uv * 2.0 - 1.0, depth, 1.0);
	float4 worldPos = mul(frameData.mainCamera.inverseViewProjection, clipPos);
	return worldPos.xyz / worldPos.w;
}

float3 ReconstructViewPos(float2 uv, float depth) {
	float4 clipPos = float4(uv * 2.0 - 1.0, depth, 1.0);
	float4 viewPos = mul(frameData.mainCamera.inverseProjection, clipPos);
	return viewPos.xyz / viewPos.w;
}

float LinearizeDepth(float rawDepth) {
	float near = frameData.mainCamera.nearPlane;
	float far = frameData.mainCamera.farPlane;
	return near * far / (far - rawDepth * (far - near));
}

float LinearDepthToRaw(float linearDepth) {
	float near = frameData.mainCamera.nearPlane;
	float far = frameData.mainCamera.farPlane;
	return (far * (linearDepth - near)) / (linearDepth * (far - near));
}

float2 GetScreenResolution() {
	return frameData.screenResolution;
}
float2 GetTexelSize() {
	return 1.0 / frameData.screenResolution;
}
float2 PixelToUV(float2 px) {
	return px / frameData.screenResolution;
}

float GetTime() {
	return frameData.time;
}
float GetDeltaTime() {
	return frameData.deltaTime;
}
uint GetFrameIndex() {
	return frameData.frameIndex;
}

float GetExposure() {
	return frameData.exposure;
}

} // namespace Aquila::Shading
#endif // AQUILA_FRAME_DATA_SLANG
