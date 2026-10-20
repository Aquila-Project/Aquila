#include "Aquila/Graphics/Material/MaterialFactory.h"
#include "Aquila/Graphics/Shader/ShaderProgram.h"
#include "Aquila/Graphics/Shader/ShaderHotReload.h"
#include "Aquila/GFX/GfxDescriptorSet.h"
#include "Aquila/GFX/GfxContext.h"
#include "Aquila/Foundation/Macros.h"

namespace Aquila::Graphics {

MaterialFactory::~MaterialFactory() {
	if (!Shader::ShaderHotReload::is_alive()) {
		return;
	}
	for (auto &[path, entry] : m_entries) {
		if (entry.watch_id != 0) {
			Shader::ShaderHotReload::get()->unregister(entry.watch_id);
		}
	}
}

MaterialCreateInfo MaterialFactory::create_info_for(Shader::ShaderDomain domain, const MaterialData &data) {
	MaterialCreateInfo info{};
	info.type = domain == Shader::ShaderDomain::Unlit ? MaterialType::Unlit : MaterialType::Lit;
	info.color_formats = { RHI::TextureFormat::RGBA16F };
	info.cull_mode = data.double_sided ? RHI::CullMode::None : RHI::CullMode::Back;
	if (data.blend_mode == BlendMode::Transparent) {
		info.blend_enabled = true;
		info.depth_write = false;
	}
	if (domain == Shader::ShaderDomain::Fullscreen) {
		info.type = MaterialType::Custom;
		info.fullscreen = true;
		info.depth_format = RHI::TextureFormat::None;
		info.cull_mode = RHI::CullMode::None;
		info.depth_test = false;
		info.depth_write = false;
	}
	return info;
}

Ref<GFX::GfxPipeline> MaterialFactory::build_pipeline(GFX::GfxContext &ctx, Shader::ShaderProgram &program,
													  const MaterialCreateInfo &info) {
	AQUILA_ASSERT(m_scene_layout != nullptr, "MaterialFactory: scene layout was not set before creating materials");
	auto *scene_layout = &m_scene_layout->get_rhi();

	if (program.get_stage_desc(RHI::ShaderStageFlags::Vertex).spirv.empty() &&
		!program.get_stage_desc(RHI::ShaderStageFlags::Compute).spirv.empty()) {
		RHI::ComputePipelineDesc desc{};
		desc.compute_shader = program.get_stage_desc(RHI::ShaderStageFlags::Compute);
		desc.set_layouts = { scene_layout };
		if (program.m_descriptor_set_layout) {
			desc.set_layouts.push_back(&program.m_descriptor_set_layout->get_rhi());
		}
		desc.push_constants = { { RHI::ShaderStageFlags::Compute, 0, info.push_constant_size } };
		return ctx.create_compute_pipeline(desc);
	}

	RHI::GraphicsPipelineDesc desc{};
	desc.vertex_shader = program.get_stage_desc(RHI::ShaderStageFlags::Vertex);
	desc.fragment_shader = program.get_stage_desc(RHI::ShaderStageFlags::Fragment);
	desc.topology = RHI::PrimitiveTopology::TriangleList;
	desc.color_formats = info.color_formats;
	desc.depth_format = info.depth_format;
	desc.no_vertex_input = info.fullscreen;

	desc.raster.cull_mode = info.cull_mode;
	desc.raster.front_face = info.front_face;

	desc.depth_stencil.depth_test = info.depth_test;
	desc.depth_stencil.depth_write = info.depth_write;

	if (info.blend_enabled) {
		RHI::BlendAttachmentDesc blend{};
		blend.enable = true;
		blend.src_color = RHI::BlendFactor::SrcAlpha;
		blend.dst_color = RHI::BlendFactor::OneMinusSrcAlpha;
		blend.color_op = RHI::BlendOp::Add;
		blend.src_alpha = RHI::BlendFactor::Zero;
		blend.dst_alpha = RHI::BlendFactor::One;
		blend.alpha_op = RHI::BlendOp::Add;
		desc.blend_attachments.assign(info.color_formats.size(), blend);
	}

	desc.set_layouts = { scene_layout };
	if (program.m_descriptor_set_layout) {
		desc.set_layouts.push_back(&program.m_descriptor_set_layout->get_rhi());
	}

	desc.push_constants = { { RHI::ShaderStageFlags::Vertex | RHI::ShaderStageFlags::Fragment, 0,
							  info.push_constant_size } };

	desc.debug_name = program.m_name;
	return ctx.create_graphics_pipeline(desc);
}

MaterialFactory::Entry *MaterialFactory::get_entry(GFX::GfxContext &ctx, const std::string &shader_path) {
	if (auto it = m_entries.find(shader_path); it != m_entries.end()) {
		return &it->second;
	}

	Entry entry;
	entry.program = Ref<Shader::ShaderProgram>(new Shader::ShaderProgram(ctx, shader_path));
	std::string err;
	const bool compiled = entry.program->add_stages_from_provider(
		[shader_path](Shader::ShaderSource &out, std::string &error) {
			return Shader::load_shader_source(shader_path, out, error);
		},
		err);
	if (!compiled) {
		AQUILA_LOG_ERROR("MaterialFactory: compile failed for '{}': {}", shader_path, err);
		return nullptr;
	}
	if (!entry.program->reflect()) {
		AQUILA_LOG_ERROR("MaterialFactory: reflection failed for '{}'", shader_path);
		return nullptr;
	}

	auto [it, inserted] = m_entries.emplace(shader_path, std::move(entry));
	const std::string &watch_path = it->second.program->get_watch_path();
	if (!watch_path.empty()) {
		it->second.watch_id =
			Shader::ShaderHotReload::get()->register_reloadable(watch_path, [this, &ctx, shader_path]() {
				auto found = m_entries.find(shader_path);
				if (found != m_entries.end()) {
					rebuild_entry(ctx, found->second, shader_path);
				}
			});
	}
	return &it->second;
}

Ref<GFX::GfxPipeline> MaterialFactory::get_pipeline(GFX::GfxContext &ctx, Entry &entry,
													const MaterialCreateInfo &info) {
	for (const auto &[existing, pipeline] : entry.pipelines) {
		if (existing == info) {
			return pipeline;
		}
	}
	Ref<GFX::GfxPipeline> pipeline = build_pipeline(ctx, *entry.program, info);
	if (pipeline) {
		entry.pipelines.emplace_back(info, pipeline);
	}
	return pipeline;
}

Ref<GFX::GfxPipeline> MaterialFactory::build_depth_pipeline(GFX::GfxContext &ctx, Entry &entry) {
	RHI::GraphicsPipelineDesc desc{};
	desc.vertex_shader = entry.depth_program->get_stage_desc(RHI::ShaderStageFlags::Vertex);
	desc.fragment_shader = entry.depth_program->get_stage_desc(RHI::ShaderStageFlags::Fragment);
	desc.topology = RHI::PrimitiveTopology::TriangleList;
	desc.color_formats = {};
	desc.depth_format = RHI::TextureFormat::Depth32;
	desc.raster.cull_mode = RHI::CullMode::None;
	desc.depth_stencil.depth_test = true;
	desc.depth_stencil.depth_write = true;
	desc.set_layouts = { &m_scene_layout->get_rhi() };
	if (entry.program->m_descriptor_set_layout) {
		desc.set_layouts.push_back(&entry.program->m_descriptor_set_layout->get_rhi());
	}
	desc.push_constants = { { RHI::ShaderStageFlags::Vertex | RHI::ShaderStageFlags::Fragment, 0, 256 } };
	desc.debug_name = entry.program->m_name + " (depth)";
	return ctx.create_graphics_pipeline(desc);
}

Ref<GFX::GfxPipeline> MaterialFactory::get_depth_pipeline(GFX::GfxContext &ctx, Entry &entry,
														  const std::string &shader_path) {
	if (entry.depth_pipeline) {
		return entry.depth_pipeline;
	}
	const Shader::ShaderDomain domain = entry.program->get_domain();
	if (domain != Shader::ShaderDomain::Lit && domain != Shader::ShaderDomain::Unlit) {
		return nullptr;
	}
	if (!entry.depth_program) {
		auto program = Ref<Shader::ShaderProgram>(new Shader::ShaderProgram(ctx, shader_path + " (depth)"));
		std::string err;
		const bool compiled = program->add_stages_from_provider(
			[shader_path](Shader::ShaderSource &out, std::string &error) {
				return Shader::load_shader_source(shader_path, out, error, Shader::ShaderVariant::Depth);
			},
			err);
		if (!compiled || !program->reflect()) {
			AQUILA_LOG_ERROR("MaterialFactory: depth variant of '{}' failed: {}", shader_path, err);
			return nullptr;
		}
		entry.depth_program = program;
	}
	entry.depth_pipeline = build_depth_pipeline(ctx, entry);
	return entry.depth_pipeline;
}

Ref<GFX::GfxPipeline> MaterialFactory::build_emissive_pipeline(GFX::GfxContext &ctx, Entry &entry, bool double_sided) {
	RHI::GraphicsPipelineDesc desc{};
	desc.vertex_shader = entry.emissive_program->get_stage_desc(RHI::ShaderStageFlags::Vertex);
	desc.fragment_shader = entry.emissive_program->get_stage_desc(RHI::ShaderStageFlags::Fragment);
	desc.topology = RHI::PrimitiveTopology::TriangleList;
	desc.color_formats = { RHI::TextureFormat::RGBA16F };
	desc.depth_format = RHI::TextureFormat::Depth32;
	desc.raster.cull_mode = double_sided ? RHI::CullMode::None : RHI::CullMode::Back;
	desc.raster.front_face = RHI::FrontFace::Clockwise;
	desc.depth_stencil.depth_test = true;
	desc.depth_stencil.depth_write = false;
	desc.depth_stencil.depth_compare = RHI::CompareOp::LessEqual;
	desc.set_layouts = { &m_scene_layout->get_rhi() };
	if (entry.program->m_descriptor_set_layout) {
		desc.set_layouts.push_back(&entry.program->m_descriptor_set_layout->get_rhi());
	}
	desc.push_constants = { { RHI::ShaderStageFlags::Vertex | RHI::ShaderStageFlags::Fragment, 0, 256 } };
	desc.debug_name = entry.program->m_name + " (emissive)";
	return ctx.create_graphics_pipeline(desc);
}

GFX::GfxPipeline *MaterialFactory::get_emissive_pipeline(GFX::GfxContext &ctx, const Material &material) {
	if (material.get_domain() != Shader::ShaderDomain::Lit) {
		return nullptr;
	}
	auto it = m_entries.find(material.get_shader_path());
	if (it == m_entries.end()) {
		return nullptr;
	}
	Entry &entry = it->second;
	const bool double_sided = material.get_data().double_sided;
	Ref<GFX::GfxPipeline> &pipeline = entry.emissive_pipelines[double_sided ? 1 : 0];
	if (pipeline || entry.emissive_failed) {
		return pipeline.get();
	}

	if (!entry.emissive_program) {
		const std::string &shader_path = it->first;
		auto program = Ref<Shader::ShaderProgram>(new Shader::ShaderProgram(ctx, shader_path + " (emissive)"));
		std::string err;
		const bool compiled = program->add_stages_from_provider(
			[shader_path](Shader::ShaderSource &out, std::string &error) {
				return Shader::load_shader_source(shader_path, out, error, Shader::ShaderVariant::Emissive);
			},
			err);
		if (!compiled || !program->reflect()) {
			AQUILA_LOG_ERROR("MaterialFactory: emissive variant of '{}' failed: {}", shader_path, err);
			entry.emissive_failed = true;
			return nullptr;
		}
		entry.emissive_program = program;
	}
	pipeline = build_emissive_pipeline(ctx, entry, double_sided);
	entry.emissive_failed = !pipeline;
	return pipeline.get();
}

Ref<Material> MaterialFactory::make(GFX::GfxContext &ctx, Entry &entry, const std::string &shader_path,
									const MaterialCreateInfo &info) {
	Ref<GFX::GfxPipeline> pipeline = get_pipeline(ctx, entry, info);
	if (!pipeline) {
		AQUILA_LOG_ERROR("MaterialFactory: pipeline creation failed for '{}'", shader_path);
		return nullptr;
	}

	Ref<Material> mat = Material::create_from_shader(ctx, *entry.program, pipeline);
	mat->set_type(info.type);
	mat->m_shader_path = shader_path;
	mat->m_uses_time = entry.program->reads_frame_time();

	entry.instances.push_back({ .material = mat, .info = info });
	return mat;
}

Ref<Material> MaterialFactory::create(GFX::GfxContext &ctx, const MaterialData &data) {
	const std::string shader_path = resolve_shader_path(data.shader);
	Entry *entry = get_entry(ctx, shader_path);
	if (entry == nullptr) {
		return nullptr;
	}
	Ref<Material> mat = make(ctx, *entry, shader_path, create_info_for(entry->program->get_domain(), data));
	if (!mat) {
		return nullptr;
	}
	mat->apply(data);
	if (data.blend_mode == BlendMode::Masked) {
		mat->m_depth_pipeline = get_depth_pipeline(ctx, *entry, shader_path);
		entry->instances.back().masked = true;
	}
	return mat;
}

Ref<Material> MaterialFactory::create(GFX::GfxContext &ctx, const std::string &shader_path, MaterialCreateInfo info) {
	Entry *entry = get_entry(ctx, shader_path);
	if (entry == nullptr) {
		return nullptr;
	}
	Ref<Material> mat = make(ctx, *entry, shader_path, info);
	if (mat) {
		MaterialData data;
		data.shader = portable_shader_path(shader_path);
		mat->apply(data);
	}
	return mat;
}

void MaterialFactory::rebuild_entry(GFX::GfxContext &ctx, Entry &entry, const std::string &shader_path) {
	std::string err;
	Ref<GFX::GfxDescriptorSetLayout> new_layout;

	if (!entry.program->reload(err, new_layout)) {
		AQUILA_LOG_ERROR("MaterialFactory: hot-reload compile failed for '{}': {}", shader_path, err);
		return;
	}
	entry.program->commit_new_layout(new_layout);

	std::vector<std::pair<MaterialCreateInfo, Ref<GFX::GfxPipeline>>> pipelines;
	for (const auto &[info, old_pipeline] : entry.pipelines) {
		Ref<GFX::GfxPipeline> pipeline = build_pipeline(ctx, *entry.program, info);
		if (!pipeline) {
			AQUILA_LOG_ERROR("MaterialFactory: hot-reload pipeline failed for '{}'", shader_path);
			return;
		}
		pipelines.emplace_back(info, pipeline);
	}

	Ref<GFX::GfxPipeline> depth_pipeline;
	if (entry.depth_program) {
		Ref<GFX::GfxDescriptorSetLayout> depth_layout;
		if (entry.depth_program->reload(err, depth_layout)) {
			entry.depth_program->commit_new_layout(depth_layout);
			depth_pipeline = build_depth_pipeline(ctx, entry);
		} else {
			AQUILA_LOG_ERROR("MaterialFactory: hot-reload of the depth variant failed for '{}': {}", shader_path, err);
		}
	}

	std::array<Ref<GFX::GfxPipeline>, 2> emissive_pipelines;
	bool emissive_failed = false;
	if (entry.emissive_program) {
		Ref<GFX::GfxDescriptorSetLayout> emissive_layout;
		if (entry.emissive_program->reload(err, emissive_layout)) {
			entry.emissive_program->commit_new_layout(emissive_layout);
			for (Usize i = 0; i < emissive_pipelines.size(); ++i) {
				if (entry.emissive_pipelines[i]) {
					emissive_pipelines[i] = build_emissive_pipeline(ctx, entry, i == 1);
				}
			}
		} else {
			AQUILA_LOG_ERROR("MaterialFactory: hot-reload of the emissive variant failed for '{}': {}", shader_path,
							 err);
			emissive_failed = true;
		}
	}

	ctx.wait_idle();
	entry.pipelines = std::move(pipelines);
	entry.depth_pipeline = depth_pipeline;
	entry.emissive_pipelines = emissive_pipelines;
	entry.emissive_failed = emissive_failed;
	if (emissive_failed) {
		entry.emissive_program.reset();
	}

	const bool uses_time = entry.program->reads_frame_time();
	auto it = entry.instances.begin();
	while (it != entry.instances.end()) {
		auto mat = it->material.lock();
		if (!mat) {
			it = entry.instances.erase(it);
			continue;
		}
		Ref<GFX::GfxPipeline> pipeline = get_pipeline(ctx, entry, it->info);
		if (pipeline) {
			mat->rebuild(*entry.program, pipeline);
			mat->m_uses_time = uses_time;
			mat->m_depth_pipeline = it->masked ? entry.depth_pipeline : nullptr;
		}
		++it;
	}

	AQUILA_LOG_INFO("MaterialFactory: hot-reloaded '{}' ({} instances)", shader_path, entry.instances.size());
}

} // namespace Aquila::Graphics
