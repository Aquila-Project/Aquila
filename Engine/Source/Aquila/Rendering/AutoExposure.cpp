#include "Aquila/Rendering/AutoExposure.h"
#include "Aquila/Foundation/EnginePaths.h"
#include "Aquila/Foundation/FrameScheduler.h"
#include "Aquila/Foundation/Macros.h"
#include "Aquila/GFX/GfxCommandList.h"
#include "Aquila/GFX/GfxContext.h"
#include "Aquila/Graphics/RenderGraph/RGGraph.h"
#include "Aquila/Graphics/RenderGraph/RGPassBuilder.h"
#include "Aquila/RHI/Vulkan/VulkanShaderCompiler.h"
#include "Aquila/Rendering/FrameContext.h"

namespace Aquila::Rendering {

using namespace Graphics;
using Aquila::Foundation::EnginePaths::shaders_dir;

namespace {

constexpr Usize K_BIN_COUNT = 256;
constexpr Uint32 K_SAMPLE_STEP = 4;
constexpr F32 K_MIN_LOG2 = -12.F;
constexpr F32 K_LOG2_RANGE = 32.F;
constexpr F64 K_LOW_PERCENTILE = 0.5;
constexpr F64 K_HIGH_PERCENTILE = 0.95;
constexpr F32 K_MIN_EV100 = -6.F;
constexpr F32 K_MAX_EV100 = 18.F;
constexpr F32 K_SPEED_BRIGHTER = 3.F;
constexpr F32 K_SPEED_DARKER = 1.2F;
constexpr F32 K_SETTLED_EV = 0.005F;
constexpr F32 K_TARGET_HYSTERESIS_EV = 0.05F;

struct HistogramPush {
	Uint32 width = 0;
	Uint32 height = 0;
	Uint32 step = K_SAMPLE_STEP;
	F32 inverse_exposure = 1.F;
	F32 min_log2 = K_MIN_LOG2;
	F32 inverse_log2_range = 1.F / K_LOG2_RANGE;
};

}

AutoExposure::AutoExposure(GFX::GfxContext &ctx) : m_ctx(ctx) {
	m_layout = ctx.create_descriptor_set_layout({
		.bindings = { {
						  .binding = 0,
						  .type = RHI::DescriptorType::CombinedImageSampler,
						  .stages = RHI::ShaderStageFlags::Compute,
						  .count = 1,
					  },
					  {
						  .binding = 1,
						  .type = RHI::DescriptorType::StorageBuffer,
						  .stages = RHI::ShaderStageFlags::Compute,
						  .count = 1,
					  } },
	});

	for (Uint32 slot = 0; slot < SharedConstants::MAX_FRAMES_IN_FLIGHT; ++slot) {
		m_histograms[slot] = ctx.create_buffer({
			.size = sizeof(Uint32) * K_BIN_COUNT,
			.usage = RHI::BufferUsage::StorageBuffer,
			.domain = RHI::MemoryDomain::GpuToCpu,
			.debug_name = "ExposureHistogram_" + std::to_string(slot),
		});
		auto *bins = static_cast<Uint32 *>(m_histograms[slot]->map());
		if (bins != nullptr) {
			std::memset(bins, 0, sizeof(Uint32) * K_BIN_COUNT);
			m_histograms[slot]->flush();
			m_histograms[slot]->unmap();
		}
		m_sets[slot] = ctx.allocate_descriptor_set(*m_layout);
		m_sets[slot]->set_buffer(1, *m_histograms[slot]);
	}

	const std::string path = shaders_dir() + "ExposureHistogram.slang";
	m_pipeline = Shader::ReloadablePipeline::create(
		ctx, path, [this, path](GFX::GfxContext &build_ctx) -> Ref<GFX::GfxPipeline> {
			std::vector<RHI::VulkanCompiledStage> stages;
			std::string err;
			if (!RHI::VulkanShaderCompiler::compile_file(path, stages, err) || stages.empty()) {
				AQUILA_LOG_ERROR("AutoExposure: shader compile failed: {}", err);
				return nullptr;
			}

			RHI::ComputePipelineDesc desc{};
			desc.compute_shader = {
				.stage = RHI::ShaderStageFlags::Compute,
				.spirv = stages[0].spirv,
				.entry_point = stages[0].entry_point_name,
			};
			desc.set_layouts = { &m_layout->get_rhi() };
			desc.push_constants = { { .stages = RHI::ShaderStageFlags::Compute,
									  .offset = 0,
									  .size = sizeof(HistogramPush) } };
			desc.debug_name = "ExposureHistogram";
			return build_ctx.create_compute_pipeline(desc);
		});
}

F32 AutoExposure::begin_frame(Uint32 frame_slot, const RenderView &view, bool measure, F32 delta_time) {
	m_slot = frame_slot;
	++m_frame;
	if (m_pending[frame_slot]) {
		read_histogram(frame_slot);
	}

	m_active = view.auto_exposure;
	if (!m_active) {
		m_ev100 = view.ev100;
		m_has_target = false;
		m_measure = false;
		m_exposure = Foundation::Photometry::exposure(view.ev100);
		return m_exposure;
	}

	m_measure = measure;
	if (m_has_target) {
		const F32 difference = m_target_ev100 - m_ev100;
		const F32 speed = difference > 0.F ? K_SPEED_BRIGHTER : K_SPEED_DARKER;
		m_ev100 += difference * (1.F - std::exp(-Math::max(delta_time, 0.F) * speed));
		if (is_settled()) {
			m_ev100 = m_target_ev100;
		} else if (measure) {
			Foundation::FrameScheduler::get()->request_frame();
		}
	}

	m_exposure = Foundation::Photometry::exposure(m_ev100 - view.exposure_compensation);
	return m_exposure;
}

void AutoExposure::add_pass(RG::RenderGraph &graph, FrameContext &ctx) {
	if (!m_active || !m_measure || !m_pipeline || !m_pipeline->is_valid() || ctx.width == 0 || ctx.height == 0) {
		return;
	}

	const Uint32 slot = m_slot;
	auto *set = m_sets[slot].get();
	auto *pipeline = m_pipeline.get();
	const RG::RGTextureHandle h_source = ctx.h_scene_color;
	RG::RGBufferHandle h_histogram = graph.import_buffer(m_histograms[slot].get(), "ExposureHistogram");

	const HistogramPush push{
		.width = ctx.width,
		.height = ctx.height,
		.inverse_exposure = 1.F / Math::max(m_exposure, 1e-30F),
	};
	const Uint32 groups_x = (((ctx.width + K_SAMPLE_STEP - 1) / K_SAMPLE_STEP) + 15) / 16;
	const Uint32 groups_y = (((ctx.height + K_SAMPLE_STEP - 1) / K_SAMPLE_STEP) + 15) / 16;

	graph.add_pass(
		"ExposureHistogram",
		[&h_histogram, h_source](RG::RGPassBuilder &builder) {
			builder.read_texture(h_source, RG::ResourceState::ShaderRead);
			h_histogram = builder.write_buffer(h_histogram);
			builder.mark_as_side_effect();
		},
		[h_source, set, pipeline, push, groups_x, groups_y](GFX::GfxCommandList &cmd, RG::RGRegistry &registry) {
			set->set_texture(0, registry.get_texture(h_source));
			set->flush();
			cmd.bind_pipeline(pipeline->get());
			cmd.bind_descriptor_set(0, *set);
			cmd.push_constants(push, RHI::ShaderStageFlags::Compute);
			cmd.dispatch(groups_x, groups_y, 1);
		});

	m_pending[slot] = true;
	m_measured_at[slot] = m_frame;
}

void AutoExposure::settle() {
	Option<Uint32> newest;
	for (Uint32 slot = 0; slot < SharedConstants::MAX_FRAMES_IN_FLIGHT; ++slot) {
		if (m_pending[slot] && (!newest || m_measured_at[slot] > m_measured_at[*newest])) {
			newest = slot;
		}
	}
	if (!newest) {
		return;
	}

	m_ctx.wait_idle();
	for (Uint32 slot = 0; slot < SharedConstants::MAX_FRAMES_IN_FLIGHT; ++slot) {
		if (m_pending[slot] && slot != *newest) {
			read_histogram(slot);
		}
	}
	read_histogram(*newest);

	if (m_active && !is_settled()) {
		Foundation::FrameScheduler::get()->request_frame();
	}
}

void AutoExposure::read_histogram(Uint32 slot) {
	m_pending[slot] = false;
	GFX::GfxBuffer &buffer = *m_histograms[slot];
	buffer.invalidate();
	auto *bins = static_cast<Uint32 *>(buffer.map());
	if (bins == nullptr) {
		return;
	}
	std::array<Uint32, K_BIN_COUNT> counts{};
	std::memcpy(counts.data(), bins, sizeof(Uint32) * K_BIN_COUNT);
	std::memset(bins, 0, sizeof(Uint32) * K_BIN_COUNT);
	buffer.flush();
	buffer.unmap();

	F64 total = 0.0;
	for (Usize i = 1; i < K_BIN_COUNT; ++i) {
		total += static_cast<F64>(counts[i]);
	}

	F32 target = K_MIN_EV100;
	if (total > 0.0) {
		const F64 low = total * K_LOW_PERCENTILE;
		const F64 high = total * K_HIGH_PERCENTILE;
		F64 seen = 0.0;
		F64 weight = 0.0;
		F64 sum = 0.0;
		for (Usize i = 1; i < K_BIN_COUNT; ++i) {
			const F64 begin = seen;
			seen += static_cast<F64>(counts[i]);
			const F64 inside = std::min(seen, high) - std::max(begin, low);
			if (inside <= 0.0) {
				continue;
			}
			const F64 log2_luminance =
				K_MIN_LOG2 + ((static_cast<F64>(i) - 0.5) / static_cast<F64>(K_BIN_COUNT - 2)) * K_LOG2_RANGE;
			sum += inside * log2_luminance;
			weight += inside;
		}
		if (weight > 0.0) {
			target = Foundation::Photometry::ev100_from_luminance(static_cast<F32>(std::exp2(sum / weight)));
		}
	}

	target = Math::clamp(target, K_MIN_EV100, K_MAX_EV100);
	if (!m_has_target) {
		m_target_ev100 = target;
		m_ev100 = target;
		m_has_target = true;
	} else if (std::abs(target - m_target_ev100) > K_TARGET_HYSTERESIS_EV) {
		m_target_ev100 = target;
	}
}

bool AutoExposure::is_settled() const {
	return !m_has_target || std::abs(m_target_ev100 - m_ev100) < K_SETTLED_EV;
}

}
