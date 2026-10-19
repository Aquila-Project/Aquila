#pragma once
#include "Aquila/Rendering/FrameContext.h"
#include "Aquila/Rendering/Systems/RenderingSystemBase.h"
#include "Aquila/Graphics/Material/Material.h"
#include "Aquila/Graphics/Shader/ReloadablePipeline.h"

namespace Aquila::Rendering {

/// Cascaded shadow maps for the sun, plus the point/spot shadow atlas SceneFrameData lays out. Adds no passes when
/// nothing casts shadows, Show > Shadows is off, or the debug view doesn't need them. Transparent materials cast no
/// shadow, masked ones draw with their material's depth pipeline so the cut out holes let light through.
class ShadowSystem : public RenderingSystemBase {
  public:
	ShadowSystem() = default;
	~ShadowSystem() override = default;

	void on_init(GFX::GfxContext &ctx) override;
	void add_passes(Graphics::RG::RenderGraph &graph, FrameContext &ctx) override;
	void on_shutdown() override;

  private:
	struct Sphere {
		Vec3 center = Vec3(0.F);
		F32 radius = 0.F;
	};

	struct MeshBounds {
		std::weak_ptr<Graphics::Resources::Mesh> mesh;
		Uint64 revision = 0;
		Sphere local;
	};

	struct Caster {
		Ref<GFX::GfxMesh> gpu_mesh;
		Mat4 model;
		Sphere bounds;
	};

	struct DrawState {
		SceneFrameData *frame_data = nullptr;
		Uint32 frame_slot = 0;
		GFX::GfxPipeline *pipeline = nullptr;
		Graphics::Material *material = nullptr;
	};

	/// Opaque casters first, then masked ones grouped by material.
	[[nodiscard]] Ref<std::vector<Caster>> collect_casters(FrameContext &ctx, bool with_bounds);
	void draw_caster(GFX::GfxCommandList &cmd, const Caster &caster, const Mat4 &view_proj, DrawState &state);
	void add_local_pass(Graphics::RG::RenderGraph &graph, FrameContext &ctx);
	[[nodiscard]] Sphere world_bounds(const Ref<Graphics::Resources::Mesh> &mesh, const Mat4 &model);

	Ref<Graphics::Shader::ReloadablePipeline> m_pipeline;
	std::unordered_map<const Graphics::Resources::Mesh *, MeshBounds> m_bounds;
};

} // namespace Aquila::Rendering
