#pragma once
#include "Aquila/Foundation/Defines.h"
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/GFX/GfxPipeline.h"

namespace Aquila::GFX {
class GfxContext;
}

namespace Aquila::Graphics::Shader {

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
