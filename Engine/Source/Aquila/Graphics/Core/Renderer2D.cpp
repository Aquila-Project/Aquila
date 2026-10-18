#include "Aquila/Graphics/Core/Renderer2D.h"
#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/Foundation/EnginePaths.h"
#include "Aquila/Foundation/Macros.h"
#include "Aquila/GFX/GfxDescriptorSet.h"
#include "Aquila/Graphics/Shader/ShaderHotReload.h"
#include "Aquila/RHI/Backend/RHITypes.h"
#include "Aquila/RHI/Vulkan/VulkanShaderCompiler.h"

using namespace Aquila;
using namespace Aquila::Graphics;

static constexpr std::string_view K_FLAT_SHADER = "2D/Flat2D.slang";
static constexpr std::string_view K_TEXTURE_SHADER = "2D/Sprite2D.slang";
static constexpr std::string_view K_GUI_SHADER = "2D/GUI2D.slang";
static constexpr std::string_view K_POINT_SHADER = "2D/Point2D.slang";
static constexpr std::string_view K_TEXT_SHADER = "2D/Text2D.slang";
static constexpr std::string_view K_SHADOW_SHADER = "2D/Shadow2D.slang";

static std::string shader_path(std::string_view file) {
	return Foundation::EnginePaths::shaders_dir() + std::string(file);
}

static constexpr Uint32 K_MAX_PRIMITIVE_VERTICES = SharedConstants::MAX_QUADS * SharedConstants::VERTS_PER_QUAD;

static constexpr Vec2 K_QUAD_UVS[4] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
static constexpr Vec4 K_LOCAL_CORNERS[4] = {
	{ -0.5F, -0.5F, 0.F, 1.F },
	{ 0.5F, -0.5F, 0.F, 1.F },
	{ 0.5F, 0.5F, 0.F, 1.F },
	{ -0.5F, 0.5F, 0.F, 1.F },
};

static Ref<GFX::GfxPipeline> build_pipeline(GFX::GfxContext &ctx, const char *shader_path,
											const std::vector<GFX::GfxDescriptorSetLayout *> &set_layouts,
											Uint32 push_constant_size, RHI::TextureFormat color_format,
											RHI::SampleCount sample_count, RHI::TextureFormat depth_format,
											RHI::VertexBindingDesc vertex_layout, RHI::PrimitiveTopology topology) {
	std::vector<RHI::VulkanCompiledStage> stages;
	std::string err;
	if (!RHI::VulkanShaderCompiler::compile_file(shader_path, stages, err)) {
		AQUILA_LOG_ERROR("Renderer2D shader failed [{}]: {}", shader_path, err);
	}

	RHI::GraphicsPipelineDesc desc{};
	for (auto &s : stages) {
		RHI::ShaderStageDesc sd{ .spirv = s.spirv, .entry_point = s.entry_point_name };
		if (s.stage == VK_SHADER_STAGE_VERTEX_BIT) {
			sd.stage = RHI::ShaderStageFlags::Vertex;
			desc.vertex_shader = sd;
		} else {
			sd.stage = RHI::ShaderStageFlags::Fragment;
			desc.fragment_shader = sd;
		}
	}

	std::vector<RHI::IRHIDescriptorSetLayout *> temp;
	temp.reserve(set_layouts.size());
	for (const auto &layout : set_layouts) {
		temp.push_back(&layout->get_rhi());
	}

	desc.set_layouts = temp;
	desc.color_formats = { color_format };
	desc.depth_format = depth_format;
	desc.topology = topology;
	desc.raster.cull_mode = RHI::CullMode::None;
	desc.raster.front_face = RHI::FrontFace::Clockwise;
	desc.depth_stencil.depth_test = false;
	desc.depth_stencil.depth_write = false;
	desc.blend_attachments = { { .enable = true } };
	desc.push_constants = { { .stages = RHI::ShaderStageFlags::Vertex, .offset = 0, .size = push_constant_size } };
	desc.sample_count = sample_count;
	desc.custom_vertex_layout = std::move(vertex_layout);

	return ctx.create_graphics_pipeline(desc);
}

static RHI::VertexBindingDesc quad_vertex_layout() {
	return RHI::VertexBindingDesc{
		.stride = sizeof(QuadVertex),
		.attributes = {
			{ 0, 0, RHI::TextureFormat::RGB32F, offsetof(QuadVertex, position) },
			{ 1, 0, RHI::TextureFormat::RGBA32F, offsetof(QuadVertex, color) },
			{ 2, 0, RHI::TextureFormat::RG32F, offsetof(QuadVertex, uv) },
			{ 3, 0, RHI::TextureFormat::RG32F, offsetof(QuadVertex, size) },
			{ 4, 0, RHI::TextureFormat::RGBA32F, offsetof(QuadVertex, radius) },
			{ 5, 0, RHI::TextureFormat::R32F, offsetof(QuadVertex, border_width) },
			{ 6, 0, RHI::TextureFormat::RGBA32F, offsetof(QuadVertex, border_color) },
			{ 7, 0, RHI::TextureFormat::R32UI, offsetof(QuadVertex, glyph_id) },
			{ 8, 0, RHI::TextureFormat::R32F, offsetof(QuadVertex, border_style) },
		},
	};
}

static RHI::VertexBindingDesc vertex_layout() {
	return RHI::VertexBindingDesc{
		.stride = sizeof(Vertex),
		.attributes = {
			{ 0, 0, RHI::TextureFormat::RG32F, offsetof(Vertex, position) },
			{ 1, 0, RHI::TextureFormat::RGBA32F, offsetof(Vertex, color) },
			{ 2, 0, RHI::TextureFormat::R32F, offsetof(Vertex, size) },
		},
	};
}

static RHI::VertexBindingDesc text_vertex_layout() {
	return RHI::VertexBindingDesc{
		.stride = sizeof(TextVertex),
		.attributes = {
			{ 0, 0, RHI::TextureFormat::RGB32F, offsetof(TextVertex, position) },
			{ 1, 0, RHI::TextureFormat::RGBA32F, offsetof(TextVertex, color) },
			{ 2, 0, RHI::TextureFormat::RG32F, offsetof(TextVertex, texcoord) },
			{ 3, 0, RHI::TextureFormat::R32F, offsetof(TextVertex, tex_loc) },
			{ 4, 0, RHI::TextureFormat::R32F, offsetof(TextVertex, band_max) },
			{ 5, 0, RHI::TextureFormat::RGBA32F, offsetof(TextVertex, banding) },
		},
	};
}

static std::vector<Uint32> generate_quad_indices(Uint32 max_quads) {
	std::vector<Uint32> indices;
	indices.reserve(max_quads * SharedConstants::INDICES_PER_QUAD);
	for (Uint32 i = 0; i < max_quads; ++i) {
		const Uint32 base = i * SharedConstants::VERTS_PER_QUAD;
		indices.push_back(base + 0);
		indices.push_back(base + 1);
		indices.push_back(base + 2);
		indices.push_back(base + 2);
		indices.push_back(base + 3);
		indices.push_back(base + 0);
	}
	return indices;
}

Renderer2D::Renderer2D(GFX::GfxContext &ctx) : m_ctx(ctx) {
	const Uint64 quad_vb_size = sizeof(QuadVertex) * SharedConstants::MAX_QUADS * SharedConstants::VERTS_PER_QUAD;
	const Uint64 text_vb_size = sizeof(TextVertex) * SharedConstants::MAX_QUADS * SharedConstants::VERTS_PER_QUAD;
	const Uint64 primitive_vb_size = sizeof(Vertex) * K_MAX_PRIMITIVE_VERTICES;

	for (Uint32 i = 0; i < K_RING_SIZE; ++i) {
		m_vertex_buffers[i] = ctx.create_buffer({
			.size = quad_vb_size,
			.usage = RHI::BufferUsage::VertexBuffer,
			.domain = RHI::MemoryDomain::CpuToGpu,
			.debug_name = "Renderer2D_VB",
		});
		m_text_vertex_buffers[i] = ctx.create_buffer({
			.size = text_vb_size,
			.usage = RHI::BufferUsage::VertexBuffer,
			.domain = RHI::MemoryDomain::CpuToGpu,
			.debug_name = "Renderer2D_TextVB",
		});
		m_primitive_vertex_buffers[i] = ctx.create_buffer({
			.size = primitive_vb_size,
			.usage = RHI::BufferUsage::VertexBuffer,
			.domain = RHI::MemoryDomain::CpuToGpu,
			.debug_name = "Renderer2D_PrimitiveVB",
		});
		m_mapped_quad_bases[i] = static_cast<QuadVertex *>(m_vertex_buffers[i]->map());
		m_mapped_primitive_bases[i] = static_cast<Vertex *>(m_primitive_vertex_buffers[i]->map());
		m_mapped_text_bases[i] = static_cast<TextVertex *>(m_text_vertex_buffers[i]->map());
	}

	auto indices = generate_quad_indices(SharedConstants::MAX_QUADS);
	m_index_buffer = ctx.create_buffer({
		.size = static_cast<Uint64>(sizeof(Uint32) * indices.size()),
		.usage = RHI::BufferUsage::IndexBuffer | RHI::BufferUsage::TransferDst,
		.domain = RHI::MemoryDomain::CpuToGpu,
		.debug_name = "Renderer2D_IB",
	});
	m_index_buffer->write(indices.data(), sizeof(Uint32) * indices.size());

	m_texture_layout = ctx.create_descriptor_set_layout({
		.bindings = { {
			.binding = 0,
			.type = RHI::DescriptorType::CombinedImageSampler,
			.stages = RHI::ShaderStageFlags::Fragment,
			.count = 1,
		} },
	});

	m_text_data_layout = ctx.create_descriptor_set_layout({
		.bindings = {
			{
				.binding = 0,
				.type    = RHI::DescriptorType::CombinedImageSampler,
				.stages  = RHI::ShaderStageFlags::Fragment,
				.count   = 1,
			},
			{
				.binding = 1,
				.type    = RHI::DescriptorType::CombinedImageSampler,
				.stages  = RHI::ShaderStageFlags::Fragment,
				.count   = 1,
			},
		},
	});

	for (auto fmt : { RHI::TextureFormat::BGRA8, RHI::TextureFormat::RGBA16F }) {
		get_or_create_flat_pipeline(fmt, RHI::SampleCount::X1, RHI::TextureFormat::None);
		get_or_create_gui_pipeline(fmt, RHI::SampleCount::X1, RHI::TextureFormat::None);
		get_or_create_point_pipeline(fmt, RHI::SampleCount::X1, RHI::TextureFormat::None);
		get_or_create_texture_pipeline(fmt, RHI::SampleCount::X1, RHI::TextureFormat::None);
		get_or_create_text_pipeline(fmt, RHI::SampleCount::X1, RHI::TextureFormat::None);
		get_or_create_shadow_pipeline(fmt, RHI::SampleCount::X1, RHI::TextureFormat::None);
	}

	get_or_create_text_pipeline(RHI::TextureFormat::BGRA8, RHI::SampleCount::X4, RHI::TextureFormat::None);

#ifdef AQUILA_HOT_RELOAD_ALL_SHADERS
	register_shader_hot_reload();
#endif
}

Renderer2D::~Renderer2D() {
	if (Shader::ShaderHotReload::is_alive()) {
		for (Uint64 id : m_watch_ids) {
			Shader::ShaderHotReload::get()->unregister(id);
		}
	}
}

void Renderer2D::register_shader_hot_reload() {
	auto *hot_reload = Shader::ShaderHotReload::get();

	m_watch_ids.push_back(hot_reload->register_reloadable(shader_path(K_FLAT_SHADER), [this]() {
		m_ctx.wait_idle();
		m_flat_pipelines.clear();
		m_last_bound_pipeline = nullptr;
	}));
	m_watch_ids.push_back(hot_reload->register_reloadable(shader_path(K_TEXTURE_SHADER), [this]() {
		m_ctx.wait_idle();
		m_texture_pipelines.clear();
		m_last_bound_pipeline = nullptr;
	}));
	m_watch_ids.push_back(hot_reload->register_reloadable(shader_path(K_GUI_SHADER), [this]() {
		m_ctx.wait_idle();
		m_gui_pipelines.clear();
		m_last_bound_pipeline = nullptr;
	}));
	m_watch_ids.push_back(hot_reload->register_reloadable(shader_path(K_TEXT_SHADER), [this]() {
		m_ctx.wait_idle();
		m_text_pipelines.clear();
		m_last_bound_pipeline = nullptr;
	}));
	m_watch_ids.push_back(hot_reload->register_reloadable(shader_path(K_SHADOW_SHADER), [this]() {
		m_ctx.wait_idle();
		m_shadow_pipelines.clear();
		m_last_bound_pipeline = nullptr;
	}));
	m_watch_ids.push_back(hot_reload->register_reloadable(shader_path(K_POINT_SHADER), [this]() {
		m_ctx.wait_idle();
		m_point_pipelines.clear();
		m_last_bound_pipeline = nullptr;
	}));
}

void Renderer2D::begin(GFX::GfxCommandList &cmd, RHI::TextureFormat color_format, RHI::SampleCount sample_count,
						const Mat4 &view_projection, RHI::TextureFormat depth_format) {
	AQUILA_ASSERT(!m_active_cmd, "Renderer2D::Begin called while already recording");
	m_active_cmd = &cmd;
	m_active_color_format = color_format;
	m_active_sample_count = sample_count;
	m_active_depth_format = depth_format;
	m_view_projection = view_projection;

	const Uint32 fi = m_frame_counter % K_RING_SIZE;
	++m_frame_counter;
	m_current_slot = fi;
	m_active_vertex_buffer = m_vertex_buffers[fi].get();
	m_active_text_vertex_buffer = m_text_vertex_buffers[fi].get();
	m_active_primitive_vertex_buffer = m_primitive_vertex_buffers[fi].get();
	m_quad_write_ptr = m_mapped_quad_bases[fi];
	m_text_write_ptr = m_mapped_text_bases[fi];
	m_vertex_write_ptr = m_mapped_primitive_bases[fi];

	m_vertex_offset = 0;
	m_text_vertex_offset = 0;
	m_primitive_vertex_offset = 0;
	m_last_bound_pipeline = nullptr;
	m_last_bound_desc_set0 = nullptr;
	m_last_bound_vertex_buffer = nullptr;
	m_push_constants_dirty = true;

	cmd.bind_index_buffer(*m_index_buffer);
	start_batch();
}

void Renderer2D::end() {
	AQUILA_ASSERT(m_active_cmd, "Renderer2D::End called without Begin");
	flush();
	if (m_capturing) {
		m_last_dirty_slot = m_current_slot;
		m_replay_quad_bytes = static_cast<Uint64>(m_vertex_offset) * sizeof(QuadVertex);
		m_replay_text_bytes = static_cast<Uint64>(m_text_vertex_offset) * sizeof(TextVertex);
		m_capturing = false;
	}
	m_active_cmd = nullptr;
}

void Renderer2D::begin_capture() {
	m_replay_list.clear();
	m_capturing = true;
}

void Renderer2D::execute_replay(GFX::GfxCommandList &cmd) {
	if (m_replay_list.empty()) {
		return;
	}

	auto replay_buffer = [this](VertexStream stream) -> GFX::GfxBuffer * {
		switch (stream) {
		case VertexStream::Text:
			return m_text_vertex_buffers[m_last_dirty_slot].get();
		case VertexStream::Primitive:
			return m_primitive_vertex_buffers[m_last_dirty_slot].get();
		case VertexStream::Quad:
			break;
		}
		return m_vertex_buffers[m_last_dirty_slot].get();
	};

	cmd.bind_index_buffer(*m_index_buffer);

	GFX::GfxPipeline *last_pipeline = nullptr;
	GFX::GfxDescriptorSet *last_desc_set = nullptr;
	GFX::GfxBuffer *last_vb = nullptr;

	for (const ReplayEntry &entry : m_replay_list) {
		if (entry.is_scissor) {
			cmd.set_scissor(entry.scissor_x, entry.scissor_y, entry.scissor_w, entry.scissor_h);
			continue;
		}

		if (entry.pipeline != last_pipeline) {
			cmd.bind_pipeline(*entry.pipeline);
			last_pipeline = entry.pipeline;
			QuadPushConstants pc{ .view_projection = m_view_projection };
			cmd.push_constants(pc, RHI::ShaderStageFlags::Vertex);
		}

		GFX::GfxBuffer *vb = replay_buffer(entry.stream);
		if (vb != last_vb) {
			cmd.bind_vertex_buffer(*vb, 0, 0);
			last_vb = vb;
		}

		if (entry.desc_set != nullptr && entry.desc_set != last_desc_set) {
			cmd.bind_descriptor_set(0, *entry.desc_set);
			last_desc_set = entry.desc_set;
		}

		if (entry.indexed) {
			cmd.draw_indexed(entry.count, 1, 0, entry.vertex_offset);
		} else {
			cmd.draw(entry.count, 1, static_cast<Uint32>(entry.vertex_offset), 0);
		}
	}
}

GFX::GfxPipeline &Renderer2D::get_or_create_pipeline(PipelineCache &cache, std::string_view shader_file,
													  GFX::GfxDescriptorSetLayout *set_layout,
													  RHI::VertexBindingDesc (*vertex_layout)(),
													  RHI::PrimitiveTopology topology, const PipelineKey &key) {
	auto it = cache.find(key);
	if (it != cache.end()) {
		return *it->second;
	}
	std::vector<GFX::GfxDescriptorSetLayout *> set_layouts;
	if (set_layout != nullptr) {
		set_layouts.push_back(set_layout);
	}
	const std::string path = shader_path(shader_file);
	cache[key] = build_pipeline(m_ctx, path.c_str(), set_layouts, sizeof(QuadPushConstants), key.color_format,
								key.sample_count, key.depth_format, vertex_layout(), topology);
	return *cache[key];
}

GFX::GfxPipeline &Renderer2D::get_or_create_flat_pipeline(RHI::TextureFormat format, RHI::SampleCount samples,
														   RHI::TextureFormat depth_format) {
	return get_or_create_pipeline(m_flat_pipelines, K_FLAT_SHADER, nullptr, quad_vertex_layout, RHI::PrimitiveTopology::TriangleList,
								  { .color_format = format, .sample_count = samples, .depth_format = depth_format });
}

GFX::GfxPipeline &Renderer2D::get_or_create_texture_pipeline(RHI::TextureFormat format, RHI::SampleCount samples,
															  RHI::TextureFormat depth_format) {
	return get_or_create_pipeline(m_texture_pipelines, K_TEXTURE_SHADER, m_texture_layout.get(), quad_vertex_layout, RHI::PrimitiveTopology::TriangleList,
								  { .color_format = format, .sample_count = samples, .depth_format = depth_format });
}

GFX::GfxPipeline &Renderer2D::get_or_create_gui_pipeline(RHI::TextureFormat format, RHI::SampleCount samples,
														  RHI::TextureFormat depth_format) {
	return get_or_create_pipeline(m_gui_pipelines, K_GUI_SHADER, nullptr, quad_vertex_layout, RHI::PrimitiveTopology::TriangleList,
								  { .color_format = format, .sample_count = samples, .depth_format = depth_format });
}

GFX::GfxPipeline &Renderer2D::get_or_create_point_pipeline(RHI::TextureFormat format, RHI::SampleCount samples,
															RHI::TextureFormat depth_format) {
	return get_or_create_pipeline(m_point_pipelines, K_POINT_SHADER, nullptr, vertex_layout,
								  RHI::PrimitiveTopology::PointList, { .color_format = format, .sample_count = samples, .depth_format = depth_format });
}

GFX::GfxPipeline &Renderer2D::get_or_create_text_pipeline(RHI::TextureFormat format, RHI::SampleCount samples,
														   RHI::TextureFormat depth_format) {
	return get_or_create_pipeline(m_text_pipelines, K_TEXT_SHADER, m_text_data_layout.get(), text_vertex_layout, RHI::PrimitiveTopology::TriangleList,
								  { .color_format = format, .sample_count = samples, .depth_format = depth_format });
}

GFX::GfxPipeline &Renderer2D::get_or_create_shadow_pipeline(RHI::TextureFormat format, RHI::SampleCount samples,
															 RHI::TextureFormat depth_format) {
	return get_or_create_pipeline(m_shadow_pipelines, K_SHADOW_SHADER, nullptr, quad_vertex_layout, RHI::PrimitiveTopology::TriangleList,
								  { .color_format = format, .sample_count = samples, .depth_format = depth_format });
}

void Renderer2D::draw_shadow(const ShadowSpec &spec) {
	if (m_batch_texture != nullptr || m_batch_type != BatchType::Shadow) {
		flush();
		start_batch();
	}
	if (m_quad_count >= SharedConstants::MAX_QUADS) {
		flush();
		start_batch();
	}
	m_batch_type = BatchType::Shadow;

	const float x0 = spec.position.x;
	const float y0 = spec.position.y;
	const float x1 = x0 + spec.size.x;
	const float y1 = y0 + spec.size.y;
	const float z = spec.depth;
	const Vec4 enc = { spec.offset.x, spec.offset.y, spec.original_half_size.x, spec.original_half_size.y };

	QuadVertex *v = m_quad_write_ptr;
	m_quad_write_ptr += 4;

	v[0] = { .position = { x0, y0, z },
			 .color = spec.color,
			 .uv = K_QUAD_UVS[0],
			 .size = spec.size,
			 .radius = spec.radius,
			 .border_width = spec.blur,
			 .border_color = enc };
	v[1] = { .position = { x1, y0, z },
			 .color = spec.color,
			 .uv = K_QUAD_UVS[1],
			 .size = spec.size,
			 .radius = spec.radius,
			 .border_width = spec.blur,
			 .border_color = enc };
	v[2] = { .position = { x1, y1, z },
			 .color = spec.color,
			 .uv = K_QUAD_UVS[2],
			 .size = spec.size,
			 .radius = spec.radius,
			 .border_width = spec.blur,
			 .border_color = enc };
	v[3] = { .position = { x0, y1, z },
			 .color = spec.color,
			 .uv = K_QUAD_UVS[3],
			 .size = spec.size,
			 .radius = spec.radius,
			 .border_width = spec.blur,
			 .border_color = enc };

	++m_quad_count;
	++m_stats.quad_count;
}

void Renderer2D::draw_rect(const RectSpec &spec) {
	const bool has_radius = glm::any(glm::greaterThan(spec.radius, Vec4(0.F)));
	const BatchType needed = (has_radius || spec.border_width > 0.F) ? BatchType::GUI : BatchType::Flat;

	if (m_batch_texture != nullptr || m_batch_type != needed) {
		flush();
		start_batch();
	}
	if (m_quad_count >= SharedConstants::MAX_QUADS) {
		flush();
		start_batch();
	}
	m_batch_type = needed;

	QuadVertex *v = m_quad_write_ptr;
	m_quad_write_ptr += 4;

	if (spec.rotation != 0.F) {
		Mat4 transform = build_quad_transform(spec.position, spec.size, spec.rotation, spec.depth);
		for (Uint32 i = 0; i < SharedConstants::VERTS_PER_QUAD; ++i) {
			Vec4 world_pos = transform * K_LOCAL_CORNERS[i];
			v[i] = { .position = Vec3(world_pos),
					 .color = spec.color,
					 .uv = K_QUAD_UVS[i],
					 .size = spec.size,
					 .radius = spec.radius,
					 .border_width = spec.border_width,
					 .border_color = spec.border_color };
		}
	} else {
		const float x0 = spec.position.x;
		const float y0 = spec.position.y;
		const float x1 = x0 + spec.size.x;
		const float y1 = y0 + spec.size.y;
		const float z = spec.depth;
		v[0] = { .position = { x0, y0, z },
				 .color = spec.color,
				 .uv = K_QUAD_UVS[0],
				 .size = spec.size,
				 .radius = spec.radius,
				 .border_width = spec.border_width,
				 .border_color = spec.border_color };
		v[1] = { .position = { x1, y0, z },
				 .color = spec.color,
				 .uv = K_QUAD_UVS[1],
				 .size = spec.size,
				 .radius = spec.radius,
				 .border_width = spec.border_width,
				 .border_color = spec.border_color };
		v[2] = { .position = { x1, y1, z },
				 .color = spec.color,
				 .uv = K_QUAD_UVS[2],
				 .size = spec.size,
				 .radius = spec.radius,
				 .border_width = spec.border_width,
				 .border_color = spec.border_color };
		v[3] = { .position = { x0, y1, z },
				 .color = spec.color,
				 .uv = K_QUAD_UVS[3],
				 .size = spec.size,
				 .radius = spec.radius,
				 .border_width = spec.border_width,
				 .border_color = spec.border_color };
	}

	for (Uint32 i = 0; i < SharedConstants::VERTS_PER_QUAD; ++i) {
		v[i].border_style = spec.border_style;
	}

	++m_quad_count;
	++m_stats.quad_count;
}

void Renderer2D::draw_sprite(const SpriteSpec &spec) {
	if (spec.texture != nullptr && !spec.texture->is_ready()) {
		return;
	}
	if (m_batch_texture != spec.texture || m_batch_type == BatchType::Point) {
		flush();
		start_batch();
		m_batch_texture = spec.texture;
		if (spec.texture != nullptr) {
			m_cached_texture_set = &get_or_create_texture_set(*spec.texture);
		}
	}
	if (m_quad_count >= SharedConstants::MAX_QUADS) {
		flush();
		start_batch();
	}

	const Vec2 uvs[4] = {
		{ spec.uv_min.x, spec.uv_min.y },
		{ spec.uv_max.x, spec.uv_min.y },
		{ spec.uv_max.x, spec.uv_max.y },
		{ spec.uv_min.x, spec.uv_max.y },
	};

	QuadVertex *v = m_quad_write_ptr;
	m_quad_write_ptr += 4;

	if (spec.rotation != 0.F) {
		Mat4 transform = build_quad_transform(spec.position, spec.size, spec.rotation, spec.depth);
		for (Uint32 i = 0; i < SharedConstants::VERTS_PER_QUAD; ++i) {
			Vec4 world_pos = transform * K_LOCAL_CORNERS[i];
			v[i] = { .position = Vec3(world_pos), .color = spec.tint, .uv = uvs[i] };
		}
	} else {
		const float x0 = spec.position.x;
		const float y0 = spec.position.y;
		const float x1 = x0 + spec.size.x;
		const float y1 = y0 + spec.size.y;
		const float z = spec.depth;
		v[0] = { .position = { x0, y0, z }, .color = spec.tint, .uv = uvs[0] };
		v[1] = { .position = { x1, y0, z }, .color = spec.tint, .uv = uvs[1] };
		v[2] = { .position = { x1, y1, z }, .color = spec.tint, .uv = uvs[2] };
		v[3] = { .position = { x0, y1, z }, .color = spec.tint, .uv = uvs[3] };
	}

	++m_quad_count;
	++m_stats.quad_count;
}

void Renderer2D::draw_point(const PointSpec &spec) {
	if (m_primitive_vertex_offset + m_primitive_batch_count >= K_MAX_PRIMITIVE_VERTICES) {
		return;
	}
	if (m_batch_type != BatchType::Point) {
		flush();
		start_batch();
		m_batch_type = BatchType::Point;
	}

	*m_vertex_write_ptr++ = { .position = spec.position, .color = spec.color, .size = spec.size };

	++m_primitive_batch_count;
	++m_stats.primitive_count;
}

void Renderer2D::draw_glyph(const GlyphSpec &spec) {
	if (m_batch_type != BatchType::Text || m_batch_curve_texture != spec.curve_texture) {
		flush();
		start_batch();
		m_batch_type = BatchType::Text;
		m_batch_curve_texture = spec.curve_texture;
		m_batch_band_texture = spec.band_texture;
		m_cached_text_data_set = &get_or_create_text_data_set(*spec.curve_texture, *spec.band_texture);
	}
	if (m_quad_count >= SharedConstants::MAX_QUADS) {
		flush();
		start_batch();
		m_batch_type = BatchType::Text;
		m_batch_curve_texture = spec.curve_texture;
		m_batch_band_texture = spec.band_texture;
		m_cached_text_data_set = &get_or_create_text_data_set(*spec.curve_texture, *spec.band_texture);
	}

	const float tex_loc = std::bit_cast<float>((spec.glyph_loc_x & 0xFFFFu) | ((spec.glyph_loc_y & 0xFFFFu) << 16u));
	const float band_max = std::bit_cast<float>((spec.band_max_x & 0xFFu) | ((spec.band_max_y & 0xFFu) << 16u));

	const float em_y0 = spec.flip_y ? spec.em_min.y : spec.em_max.y;
	const float em_y1 = spec.flip_y ? spec.em_max.y : spec.em_min.y;

	const float x0 = spec.position.x;
	const float y0 = spec.position.y;
	const float x1 = x0 + spec.size.x;
	const float y1 = y0 + spec.size.y;
	const float z = spec.depth;

	TextVertex *v = m_text_write_ptr;
	m_text_write_ptr += 4;

	v[0] = { .position = { x0, y0, z },
			 .color = spec.color,
			 .texcoord = { spec.em_min.x, em_y0 },
			 .tex_loc = tex_loc,
			 .band_max = band_max,
			 .banding = spec.banding };
	v[1] = { .position = { x1, y0, z },
			 .color = spec.color,
			 .texcoord = { spec.em_max.x, em_y0 },
			 .tex_loc = tex_loc,
			 .band_max = band_max,
			 .banding = spec.banding };
	v[2] = { .position = { x1, y1, z },
			 .color = spec.color,
			 .texcoord = { spec.em_max.x, em_y1 },
			 .tex_loc = tex_loc,
			 .band_max = band_max,
			 .banding = spec.banding };
	v[3] = { .position = { x0, y1, z },
			 .color = spec.color,
			 .texcoord = { spec.em_min.x, em_y1 },
			 .tex_loc = tex_loc,
			 .band_max = band_max,
			 .banding = spec.banding };

	++m_quad_count;
	++m_stats.quad_count;
}

void Renderer2D::flush() {
	if (m_quad_count == 0 && m_primitive_batch_count == 0) {
		return;
	}

	auto bind_pipeline_if_changed = [&](GFX::GfxPipeline &pipeline) {
		if (&pipeline != m_last_bound_pipeline) {
			m_active_cmd->bind_pipeline(pipeline);
			m_last_bound_pipeline = &pipeline;

			m_push_constants_dirty = true;
		}
	};

	auto bind_desc_set0_if_changed = [&](GFX::GfxDescriptorSet &set) {
		if (&set != m_last_bound_desc_set0) {
			m_active_cmd->bind_descriptor_set(0, set);
			m_last_bound_desc_set0 = &set;
		}
	};

	auto bind_vertex_buffer_if_changed = [&](GFX::GfxBuffer &buf) {
		if (&buf != m_last_bound_vertex_buffer) {
			m_active_cmd->bind_vertex_buffer(buf, 0, 0);
			m_last_bound_vertex_buffer = &buf;
		}
	};

	if (m_batch_type == BatchType::Point) {
		bind_pipeline_if_changed(
			get_or_create_point_pipeline(m_active_color_format, m_active_sample_count, m_active_depth_format));

		if (m_push_constants_dirty) {
			QuadPushConstants pc{ .view_projection = m_view_projection };
			m_active_cmd->push_constants(pc, RHI::ShaderStageFlags::Vertex);
			m_push_constants_dirty = false;
		}

		bind_vertex_buffer_if_changed(*m_active_primitive_vertex_buffer);

		const Uint32 vertex_count = m_primitive_batch_count;
		const Uint32 first_vertex = m_primitive_vertex_offset;
		m_active_cmd->draw(vertex_count, 1, first_vertex, 0);
		++m_stats.draw_calls;

		if (m_capturing) {
			m_replay_list.push_back({ .pipeline = m_last_bound_pipeline,
									  .desc_set = nullptr,
									  .stream = VertexStream::Primitive,
									  .indexed = false,
									  .count = vertex_count,
									  .vertex_offset = static_cast<Int32>(first_vertex) });
		}

		m_primitive_vertex_offset += vertex_count;
	} else if (m_batch_type == BatchType::Text) {
		bind_pipeline_if_changed(
			get_or_create_text_pipeline(m_active_color_format, m_active_sample_count, m_active_depth_format));

		if (m_push_constants_dirty) {
			QuadPushConstants pc{ .view_projection = m_view_projection };
			m_active_cmd->push_constants(pc, RHI::ShaderStageFlags::Vertex);
			m_push_constants_dirty = false;
		}

		bind_vertex_buffer_if_changed(*m_active_text_vertex_buffer);
		bind_desc_set0_if_changed(*m_cached_text_data_set);

		const Uint32 index_count = m_quad_count * SharedConstants::INDICES_PER_QUAD;
		const Int32 vertex_off = static_cast<Int32>(m_text_vertex_offset);
		m_active_cmd->draw_indexed(index_count, 1, 0, vertex_off);
		++m_stats.draw_calls;

		if (m_capturing) {
			m_replay_list.push_back({ .pipeline = m_last_bound_pipeline,
									  .desc_set = m_cached_text_data_set,
									  .stream = VertexStream::Text,
									  .indexed = true,
									  .count = index_count,
									  .vertex_offset = vertex_off });
		}

		m_text_vertex_offset += m_quad_count * SharedConstants::VERTS_PER_QUAD;
	} else {
		GFX::GfxPipeline &pipeline = [&]() -> GFX::GfxPipeline & {
			if (m_batch_texture != nullptr) {
				return get_or_create_texture_pipeline(m_active_color_format, m_active_sample_count,
													  m_active_depth_format);
			}
			if (m_batch_type == BatchType::GUI) {
				return get_or_create_gui_pipeline(m_active_color_format, m_active_sample_count, m_active_depth_format);
			}
			if (m_batch_type == BatchType::Shadow) {
				return get_or_create_shadow_pipeline(m_active_color_format, m_active_sample_count,
													 m_active_depth_format);
			}
			return get_or_create_flat_pipeline(m_active_color_format, m_active_sample_count, m_active_depth_format);
		}();

		bind_pipeline_if_changed(pipeline);

		if (m_push_constants_dirty) {
			QuadPushConstants pc{ .view_projection = m_view_projection };
			m_active_cmd->push_constants(pc, RHI::ShaderStageFlags::Vertex);
			m_push_constants_dirty = false;
		}

		bind_vertex_buffer_if_changed(*m_active_vertex_buffer);

		GFX::GfxDescriptorSet *bound_set = nullptr;
		if (m_batch_texture != nullptr) {
			bind_desc_set0_if_changed(*m_cached_texture_set);
			bound_set = m_cached_texture_set;
		}

		const Uint32 index_count = m_quad_count * SharedConstants::INDICES_PER_QUAD;
		const Int32 vertex_off = static_cast<Int32>(m_vertex_offset);
		m_active_cmd->draw_indexed(index_count, 1, 0, vertex_off);
		++m_stats.draw_calls;

		if (m_capturing) {
			m_replay_list.push_back({ .pipeline = m_last_bound_pipeline,
									  .desc_set = bound_set,
									  .stream = VertexStream::Quad,
									  .indexed = true,
									  .count = index_count,
									  .vertex_offset = vertex_off });
		}

		m_vertex_offset += m_quad_count * SharedConstants::VERTS_PER_QUAD;
	}
	start_batch();
}

GFX::GfxDescriptorSet &Renderer2D::get_or_create_texture_set(GFX::GfxTexture &texture) {
	auto it = m_texture_set_cache.find(&texture);
	if (it != m_texture_set_cache.end()) {
		return *it->second;
	}
	auto set = m_ctx.allocate_descriptor_set(*m_texture_layout);
	set->set_texture(0, texture);
	set->flush();
	m_texture_set_cache[&texture] = std::move(set);
	return *m_texture_set_cache[&texture];
}

GFX::GfxDescriptorSet &Renderer2D::get_or_create_text_data_set(GFX::GfxTexture &curve_texture,
																GFX::GfxTexture &band_texture) {
	auto it = m_text_data_set_cache.find(&curve_texture);
	if (it != m_text_data_set_cache.end()) {
		return *it->second;
	}
	auto set = m_ctx.allocate_descriptor_set(*m_text_data_layout);
	set->set_texture(0, curve_texture);
	set->set_texture(1, band_texture);
	set->flush();
	m_text_data_set_cache[&curve_texture] = std::move(set);
	return *m_text_data_set_cache[&curve_texture];
}

void Renderer2D::start_batch() {
	m_quad_count = 0;
	m_primitive_batch_count = 0;
	m_batch_type = BatchType::Flat;
	m_batch_texture = nullptr;
	m_batch_curve_texture = nullptr;
	m_batch_band_texture = nullptr;
}

void Renderer2D::set_scissor(GFX::GfxCommandList &cmd, Int32 x, Int32 y, Uint32 w, Uint32 h) {
	cmd.set_scissor(x, y, w, h);
	if (m_capturing) {
		m_replay_list.push_back({
			.pipeline = nullptr,
			.desc_set = nullptr,
			.stream = VertexStream::Quad,
			.indexed = false,
			.count = 0,
			.vertex_offset = 0,
			.is_scissor = true,
			.scissor_x = x,
			.scissor_y = y,
			.scissor_w = w,
			.scissor_h = h,
		});
	}
}

Mat4 Renderer2D::build_quad_transform(Vec2 position, Vec2 size, float rotation, float depth) const {
	Vec2 center = position + size * 0.5F;
	Mat4 t = glm::translate(Mat4(1.F), Vec3(center, depth));
	if (rotation != 0.F) {
		t = glm::rotate(t, rotation, Vec3(0.F, 0.F, 1.F));
	}
	t = glm::scale(t, Vec3(size, 1.F));
	return t;
}
