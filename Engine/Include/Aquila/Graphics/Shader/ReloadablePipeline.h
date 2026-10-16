#pragma once
#include "Aquila/Foundation/Defines.h"
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/GFX/GfxPipeline.h"

namespace Aquila::GFX {
class GfxContext;
}

namespace Aquila::Graphics::Shader {

/// A pipeline that rebuilds itself when its shader file changes (debug builds only).
/// `build` runs again on every change. If it returns null the old pipeline stays.
///
/// @code
/// auto grid = ReloadablePipeline::create(ctx, grid_shader_path, [this](GFX::GfxContext &ctx) {
///     return build_grid_pipeline(ctx);
/// });
/// grid->get().bind(cmd);
/// @endcode
class ReloadablePipeline {
  public:
	using BuildFn = Delegate<Ref<GFX::GfxPipeline>(GFX::GfxContext &)>;

	static Ref<ReloadablePipeline> create(GFX::GfxContext &ctx, std::string shader_path, BuildFn build);
	~ReloadablePipeline();

	AQUILA_NONCOPYABLE(ReloadablePipeline);
	AQUILA_NONMOVEABLE(ReloadablePipeline);

	[[nodiscard]] bool is_valid() const { return m_pipeline != nullptr; }
	[[nodiscard]] GFX::GfxPipeline &get() const { return *m_pipeline; }
	explicit operator bool() const { return m_pipeline != nullptr; }

	void rebuild();

  private:
	ReloadablePipeline(GFX::GfxContext &ctx, std::string shader_path, BuildFn build);

	GFX::GfxContext &m_ctx;
	std::string m_shader_path;
	BuildFn m_build;
	Ref<GFX::GfxPipeline> m_pipeline;
	[[maybe_unused]] Uint64 m_watch_id = 0;
};

} // namespace Aquila::Graphics::Shader
