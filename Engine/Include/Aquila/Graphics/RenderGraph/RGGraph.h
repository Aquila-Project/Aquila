#pragma once
#include "Aquila/Graphics/RenderGraph/RGRegistry.h"
#include "Aquila/Graphics/RenderGraph/RGPassBuilder.h"
#include "Aquila/Graphics/RenderGraph/RGCompiler.h"
#include "Aquila/RHI/Backend/IRHIQueryPool.h"

namespace Aquila::GFX {
class GfxContext;
class GfxCommandList;
} // namespace Aquila::GFX

namespace Aquila::Graphics::RG {

/// Passes declare what they read and write, the graph orders them, drops the ones nothing uses,
/// allocates transient resources and puts the barriers in.
///
/// @code
/// RGTextureHandle color = graph.declare_texture({ .width = w, .height = h, .format = RHI::TextureFormat::RGBA16F });
/// graph.add_pass(
///     "Sky",
///     [&](RGPassBuilder &builder) { color = builder.set_color_attachment(0, color); },
///     [](GFX::GfxCommandList &cmd, RGRegistry &) { cmd.draw(3); });
/// graph.compile(ctx);
/// graph.execute(cmd);
/// graph.reset();
/// @endcode
class RenderGraph {
  public:
	RenderGraph() = default;
	~RenderGraph() = default;

	RenderGraph(const RenderGraph &) = delete;
	RenderGraph &operator=(const RenderGraph &) = delete;
	RenderGraph(RenderGraph &&) = default;
	RenderGraph &operator=(RenderGraph &&) = default;

	/// Transient: the graph creates it at compile time and may reuse a texture another pass is done with.
	RGTextureHandle declare_texture(const RGTextureDesc &desc) { return m_registry.declare_texture(desc); }

	RGBufferHandle declare_buffer(const RGBufferDesc &desc) { return m_registry.declare_buffer(desc); }

	/// The graph doesn't own it, keep it alive until execute() is done.
	/// `initial_state` is the state it's in when the frame starts.
	RGTextureHandle import_texture(GFX::GfxTexture *tex, std::string_view name = {},
								  RG::ResourceState initial_state = RG::ResourceState::Undefined) {
		return m_registry.import_texture(tex, name, initial_state);
	}

	RGBufferHandle import_buffer(GFX::GfxBuffer *buf, std::string_view name = {},
								RG::ResourceState initial_state = RG::ResourceState::Undefined) {
		return m_registry.import_buffer(buf, name, initial_state);
	}

	/// Register a pass with a setup lambda and an execute lambda.
	///
	/// @param name        Debug label shown in GPU profilers / validation.
	/// @param setupFn     Called immediately (sync) to declare resource accesses.
	///                    Receives an RGPassBuilder by reference.
	/// @param executeFn   Captured and called later during Execute().
	///                    Receives a resolved command list and the registry.
	template <typename SetupFn, typename ExecuteFn>
	void add_pass(std::string_view name, SetupFn &&setup_fn, ExecuteFn &&execute_fn) {
		RGPassBuilder builder(name, m_registry);

		// Run setup immediately so resource versioning stays in-order.
		std::forward<SetupFn>(setup_fn)(builder);

		RGPassData data = std::move(builder).take_data();
		data.render_pass_execute = std::forward<ExecuteFn>(execute_fn);

		m_passes.push_back(std::move(data));
	}

	/// Must be called once per frame after all AddPass calls and before Execute().
	void compile(GFX::GfxContext &ctx);

	/// Replay the compiled schedule.
	/// Compile() must have been called first.
	void execute(GFX::GfxCommandList &cmd);

	/// With a pool, execute() writes a timestamp before and after every pass.
	/// get_timed_passes() names them in the same order.
	void set_timestamp_pool(RHI::IRHIQueryPool *pool) { m_timestamp_pool = pool; }
	[[nodiscard]] const std::vector<std::string> &get_timed_passes() const { return m_timed_passes; }

	/// Reset all state for the next frame (releases transient resources).
	void reset();

	[[nodiscard]] const RGRegistry &get_registry() const { return m_registry; }
	[[nodiscard]] const std::vector<RGPassData> &get_passes() const { return m_passes; }
	[[nodiscard]] const RGCompiledGraph &get_compiled() const { return m_compiled; }

  private:
	RGRegistry m_registry;
	std::vector<RGPassData> m_passes;
	RGCompiledGraph m_compiled;
	RHI::IRHIQueryPool *m_timestamp_pool = nullptr;
	std::vector<std::string> m_timed_passes;
};

} // namespace Aquila::Graphics::RG
