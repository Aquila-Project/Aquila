#pragma once
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Graphics/RenderGraph/RGTypes.h"
#include "Aquila/Rendering/LightData.h"
#include "Aquila/Rendering/RenderSettings.h"

namespace Aquila::SceneManagement {
class Scene;
}

namespace Aquila::GFX {
class GfxSwapchain;
}

namespace Aquila::Rendering {

class SceneFrameData;

/// What a system gets every frame. The h_* handles are how systems pass graph resources to each other:
/// a system that writes one must store the new handle back, so the next system reads the latest version.
struct FrameContext {
	SceneManagement::Scene *scene = nullptr;

	Mat4 view = Mat4(1.F);
	Mat4 projection = Mat4(1.F);
	Mat4 view_projection = Mat4(1.F);
	Vec3 camera_position = {};

	Graphics::RG::RGTextureHandle h_scene_color;
	Graphics::RG::RGTextureHandle h_depth;
	Graphics::RG::RGTextureHandle h_object_picking;

	std::array<Graphics::RG::RGTextureHandle, SHADOW_CASCADE_COUNT> h_shadow_maps;

	Graphics::RG::RGBufferHandle h_cluster_aab_bs;
	Graphics::RG::RGBufferHandle h_light_list;
	Graphics::RG::RGBufferHandle h_cluster_light_info;

	GFX::GfxSwapchain *swapchain = nullptr;
	Uint32 swapchain_image_index = 0;
	bool swapchain_replaced = false;

	Uint32 width = 0;
	Uint32 height = 0;
	F32 delta_time = 0.F;

	SceneFrameData *frame_data = nullptr;
	Uint32 frame_slot = 0;

	const RenderSettings *settings = nullptr;
};

} // namespace Aquila::Rendering
