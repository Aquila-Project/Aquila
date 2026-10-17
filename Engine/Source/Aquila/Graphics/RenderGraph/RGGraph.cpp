#include "Aquila/Graphics/RenderGraph/RGGraph.h"
#include "Aquila/Graphics/RenderGraph/RGCompiler.h"
#include "Aquila/Foundation/Macros.h"
#include "Aquila/GFX/GfxCommandList.h"
#include "Aquila/GFX/GfxRenderpass.h"
#include "Aquila/GFX/GfxContext.h"
#include "Aquila/GFX/GfxTexture.h"
#include "Aquila/GFX/GfxBuffer.h"

namespace Aquila::Graphics::RG {

void RenderGraph::compile(GFX::GfxContext &ctx) {
	AQUILA_ASSERT(!m_passes.empty(), "RenderGraph::Compile called with no passes registered");
	m_compiled.reset();
	m_compiled = RGCompiler::compile(m_passes, m_registry, ctx);
}

void RenderGraph::execute(GFX::GfxCommandList &cmd) {
	AQUILA_ASSERT(m_compiled.valid, "RenderGraph::Execute called before Compile()");

	Uint32 sched_pos = 0;

	m_timed_passes.clear();
	const Uint32 timed_capacity = m_timestamp_pool != nullptr ? m_timestamp_pool->get_count() / 2 : 0;
	const Uint32 timed_count = std::min(timed_capacity, static_cast<Uint32>(m_compiled.pass_order.size()));
	if (timed_count > 0) {
		cmd.reset_queries(*m_timestamp_pool, 0, timed_count * 2);
		m_timed_passes.reserve(timed_count);
	}

	for (const Uint32 pi : m_compiled.pass_order) {
		const RGPassData &pass = m_passes[pi];
		AQUILA_ASSERT(pass.render_pass_execute, "A pass has no execute function");

		const bool timed = sched_pos < timed_count;
		if (timed) {
			cmd.write_timestamp(*m_timestamp_pool, sched_pos * 2);
			m_timed_passes.push_back(pass.name);
		}

		cmd.push_debug_group(pass.name.c_str());

		// Pre-pass texture barriers
		const Uint32 tex_bar_begin = m_compiled.pass_tex_bar_start[sched_pos];
		const Uint32 tex_bar_end = m_compiled.pass_tex_bar_start[sched_pos + 1];
		for (Uint32 bi = tex_bar_begin; bi < tex_bar_end; ++bi) {
			const RGTexBarrier &bar = m_compiled.tex_barriers[bi];
			GFX::GfxTexture &tex = m_registry.get_texture(bar.handle);
			cmd.transition_texture(tex, bar.old_state, bar.new_state);
		}

		//  Pre-pass buffer barriers
		const Uint32 buf_bar_begin = m_compiled.pass_buf_bar_start[sched_pos];
		const Uint32 buf_bar_end = m_compiled.pass_buf_bar_start[sched_pos + 1];
		for (Uint32 bi = buf_bar_begin; bi < buf_bar_end; ++bi) {
			const RGBufBarrier &bar = m_compiled.buf_barriers[bi];
			GFX::GfxBuffer &buf = m_registry.get_buffer(bar.handle);
			cmd.transition_buffer(buf, bar.old_state, bar.new_state);
		}

		// Begin renderpass (graphics passes only)
		GFX::GfxRenderPass *render_pass = m_compiled.pass_render_passes[sched_pos].get();
		if (render_pass != nullptr) {
			render_pass->begin(cmd);
		}

		// User execute callback
		pass.render_pass_execute(cmd, m_registry);

		// End renderpass
		if (render_pass != nullptr) {
			render_pass->end(cmd);
		}

		cmd.pop_debug_group();
		if (timed) {
			cmd.write_timestamp(*m_timestamp_pool, (sched_pos * 2) + 1);
		}
		++sched_pos;
	}
}

void RenderGraph::reset() {
	m_compiled.reset();
	m_passes.clear();
	m_registry.reset();
}

} // namespace Aquila::Graphics::RG
