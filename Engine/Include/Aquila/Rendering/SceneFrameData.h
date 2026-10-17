#pragma once
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/Foundation/Singleton.h"
#include "Aquila/GFX/GfxBuffer.h"
#include "Aquila/GFX/GfxTexture.h"
#include "Aquila/GFX/GfxDescriptorSet.h"
#include "Aquila/Rendering/LightData.h"
#include "Aquila/Rendering/RenderView.h"

namespace Aquila::GFX {
class GfxContext;
}

namespace Aquila::SceneManagement {
class Scene;
}

namespace Aquila::SceneManagement::Components {
struct CameraComponent;
struct TransformComponent;
} // namespace Aquila::SceneManagement::Components

namespace Aquila::Rendering {

RenderView render_view_from_entity(const SceneManagement::Components::CameraComponent &cam,
								   const SceneManagement::Components::TransformComponent &transform);

/// Uploads the per-frame scene data (camera, lights, materials, shadows, environment) and owns
/// the scene descriptor set every material binds.
class SceneFrameData : public Foundation::Singleton<SceneFrameData> {
  public:
	SceneFrameData(GFX::GfxContext &ctx, Uint32 width, Uint32 height);

	/// Fills `frame_slot`'s buffers. Also sets MaterialComponent::material_index on every material.
	void update(SceneManagement::Scene &scene, float delta_time, Uint32 frame_slot, const RenderView &primary_view,
				bool shadows_allowed = true);

	void on_resize(Uint32 width, Uint32 height);

	[[nodiscard]] GFX::GfxDescriptorSet &get_descriptor_set(Uint32 frame_slot) const;
	[[nodiscard]] GFX::GfxDescriptorSetLayout &get_layout() const { return *m_layout; }

	[[nodiscard]] GFX::GfxBuffer &get_light_index_list_buffer() const { return *m_light_index_list_buffer; }
	[[nodiscard]] GFX::GfxBuffer &get_cluster_light_info_buffer() const { return *m_cluster_light_info_buffer; }

	static constexpr Uint32 kShadowMapSize = 2048;

	[[nodiscard]] GFX::GfxTexture &get_shadow_map(Uint32 frame_slot, Uint32 cascade) const {
		return *m_shadow_maps[frame_slot][cascade];
	}
	[[nodiscard]] const std::array<Mat4, SHADOW_CASCADE_COUNT> &get_cascade_view_proj() const {
		return m_cascade_view_proj;
	}
	[[nodiscard]] bool shadows_enabled() const { return m_shadows_enabled; }

  private:
	GFX::GfxContext &m_ctx;
	Uint32 m_width = 0;
	Uint32 m_height = 0;
	Uint32 m_frame_index = 0;
	float m_time = 0.F;

	Ref<GFX::GfxDescriptorSetLayout> m_layout;

	std::array<Ref<GFX::GfxBuffer>, SharedConstants::MAX_FRAMES_IN_FLIGHT> m_frame_buffers;

	std::array<Ref<GFX::GfxBuffer>, SharedConstants::MAX_FRAMES_IN_FLIGHT> m_light_buffers;

	std::array<Ref<GFX::GfxBuffer>, SharedConstants::MAX_FRAMES_IN_FLIGHT> m_env_buffers;

	std::array<Ref<GFX::GfxBuffer>, SharedConstants::MAX_FRAMES_IN_FLIGHT> m_material_buffers;

	Ref<GFX::GfxBuffer> m_light_index_list_buffer;

	Ref<GFX::GfxBuffer> m_cluster_light_info_buffer;

	std::array<Ref<GFX::GfxBuffer>, SharedConstants::MAX_FRAMES_IN_FLIGHT> m_shadow_buffers;

	std::array<std::array<Ref<GFX::GfxTexture>, SHADOW_CASCADE_COUNT>, SharedConstants::MAX_FRAMES_IN_FLIGHT>
		m_shadow_maps;

	std::array<Mat4, SHADOW_CASCADE_COUNT> m_cascade_view_proj{};

	bool m_shadows_enabled = false;

	std::array<Ref<GFX::GfxDescriptorSet>, SharedConstants::MAX_FRAMES_IN_FLIGHT> m_sets;
};

} // namespace Aquila::Rendering
