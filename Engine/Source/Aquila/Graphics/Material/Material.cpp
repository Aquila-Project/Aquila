#include "Aquila/Graphics/Material/Material.h"
#include "Aquila/Graphics/Shader/ShaderProgram.h"
#include "Aquila/GFX/GfxContext.h"
#include "Aquila/Foundation/SharedConstants.h"

namespace Aquila::Graphics {

static Uint32 std140_align(ParameterType t) {
	switch (t) {
	case ParameterType::Float:
	case ParameterType::Int:
	case ParameterType::Bool:
		return 4;
	case ParameterType::Vec2:
		return 8;
	case ParameterType::Vec3:
	case ParameterType::Vec4:
	case ParameterType::Color:
		return 16;
	default:
		return 4;
	}
}

// Bytes occupied in the UBO stream (vec3 has a 16-byte stride in std140).
static Uint32 std140_stride(ParameterType t) {
	switch (t) {
	case ParameterType::Float:
	case ParameterType::Int:
	case ParameterType::Bool:
		return 4;
	case ParameterType::Vec2:
		return 8;
	case ParameterType::Vec3:
		return 16; // 12 bytes of data, 4 bytes pad
	case ParameterType::Vec4:
	case ParameterType::Color:
		return 16;
	default:
		return 4;
	}
}

// Bytes actually copied when writing the value (no padding).
static Uint32 value_size(ParameterType t) {
	switch (t) {
	case ParameterType::Float:
	case ParameterType::Int:
	case ParameterType::Bool:
		return 4;
	case ParameterType::Vec2:
		return 8;
	case ParameterType::Vec3:
		return 12;
	case ParameterType::Vec4:
	case ParameterType::Color:
		return 16;
	default:
		return 4;
	}
}

static ParameterValue default_value(ParameterType t) {
	switch (t) {
	case ParameterType::Float:
		return 0.F;
	case ParameterType::Int:
		return 0;
	case ParameterType::Bool:
		return false;
	case ParameterType::Vec2:
		return Vec2{ 0.F, 0.F };
	case ParameterType::Vec3:
		return Vec3{ 0.F, 0.F, 0.F };
	case ParameterType::Vec4:
	case ParameterType::Color:
		return Vec4{ 0.F, 0.F, 0.F, 1.F };
	default:
		return 0.F;
	}
}

Ref<Material> Material::create_from_shader(GFX::GfxContext &ctx, Shader::ShaderProgram &shader,
										   Ref<GFX::GfxPipeline> pipeline) {
	auto mat = Ref<Material>(new Material());
	mat->m_context = &ctx;
	mat->m_pipeline = std::move(pipeline);
	mat->m_layout = shader.m_descriptor_set_layout;

	if (mat->m_layout) {
		for (Uint32 i = 0; i < SharedConstants::MAX_FRAMES_IN_FLIGHT; ++i) {
			mat->m_sets[i] = ctx.allocate_descriptor_set(*mat->m_layout);
		}
	}

	using RBType = Shader::ShaderProgram::ReflectedBindingType;

	Uint32 ubo_size = 0;

	for (const auto &binding : shader.get_reflected_bindings()) {
		if (binding.set != 1) {
			continue;
		}
		if (binding.type == RBType::UniformBuffer) {
			Uint32 offset = 0;
			for (const auto &field : binding.ubo_fields) {
				Uint32 align = std140_align(field.param_type);
				offset = (offset + align - 1) & ~(align - 1);

				MaterialParameter p;
				p.name = field.name;
				p.type = field.param_type;
				p.ubo_offset = offset;
				p.value = default_value(field.param_type);
				p.default_value = p.value;
				mat->m_parameters.push_back(std::move(p));

				offset += std140_stride(field.param_type);
			}
			ubo_size = std::max(ubo_size, offset);
		} else if (binding.type == RBType::CombinedImageSampler) {
			MaterialParameter p;
			p.name = binding.name;
			p.type = ParameterType::Texture2D;
			p.texture_binding = binding.binding_index;
			mat->m_parameters.push_back(std::move(p));
		}
	}

	// std140 struct size must be a multiple of 16.
	ubo_size = (ubo_size + 15u) & ~15u;

	if (ubo_size > 0 && mat->m_sets[0]) {
		mat->m_ubo_data.assign(ubo_size, 0);
		for (Uint32 i = 0; i < SharedConstants::MAX_FRAMES_IN_FLIGHT; ++i) {
			mat->m_uniform_buffers[i] = ctx.create_buffer({
				.size = ubo_size,
				.usage = RHI::BufferUsage::UniformBuffer,
				.domain = RHI::MemoryDomain::CpuToGpu,
				.debug_name = ("MaterialUBO_" + std::to_string(i)).c_str(),
			});
		}
		mat->m_dirty_slot_mask = (1u << SharedConstants::MAX_FRAMES_IN_FLIGHT) - 1u;
	}

	return mat;
}

Ref<Material> Material::create(GFX::GfxContext &ctx, Ref<GFX::GfxPipeline> pipeline,
							   Ref<GFX::GfxDescriptorSetLayout> layout) {
	auto mat = Ref<Material>(new Material());
	mat->m_context = &ctx;
	mat->m_pipeline = std::move(pipeline);
	mat->m_layout = layout;

	if (mat->m_layout) {
		for (Uint32 i = 0; i < SharedConstants::MAX_FRAMES_IN_FLIGHT; ++i) {
			mat->m_sets[i] = ctx.allocate_descriptor_set(*mat->m_layout);
		}
	}

	return mat;
}

void Material::register_parameter(std::string param_name, ParameterType type, Uint32 ubo_offset,
								  Uint32 texture_binding) {
	for (auto &p : m_parameters) {
		if (p.name == param_name) {
			p.type = type;
			p.ubo_offset = ubo_offset;
			p.texture_binding = texture_binding;
			return;
		}
	}
	MaterialParameter p;
	p.name = std::move(param_name);
	p.type = type;
	p.ubo_offset = ubo_offset;
	p.texture_binding = texture_binding;
	p.value = default_value(type);
	p.default_value = p.value;
	m_parameters.push_back(std::move(p));
}

MaterialParameter *Material::find_parameter(const std::string &param_name) {
	for (auto &p : m_parameters) {
		if (p.name == param_name) {
			return &p;
		}
	}
	return nullptr;
}

const MaterialParameter *Material::get_parameter(const std::string &param_name) const {
	for (const auto &p : m_parameters) {
		if (p.name == param_name) {
			return &p;
		}
	}
	return nullptr;
}

template <typename T> void Material::write_ubo(Uint32 offset, const T &v) {
	if (m_ubo_data.empty()) {
		Uint32 needed = offset + static_cast<Uint32>(sizeof(T));
		if (needed > m_ubo_data.size()) {
			m_ubo_data.resize((needed + 15u) & ~15u, 0);
		}
	}
	if (offset + sizeof(T) <= m_ubo_data.size()) {
		std::memcpy(m_ubo_data.data() + offset, &v, sizeof(T));
	}
}

void Material::ensure_uniform_buffers() {
	if (m_uniform_buffers[0] || m_ubo_data.empty() || (m_context == nullptr) || !m_sets[0]) {
		return;
	}
	Uint32 size = static_cast<Uint32>(m_ubo_data.size());
	for (Uint32 i = 0; i < SharedConstants::MAX_FRAMES_IN_FLIGHT; ++i) {
		m_uniform_buffers[i] = m_context->create_buffer({
			.size = size,
			.usage = RHI::BufferUsage::UniformBuffer,
			.domain = RHI::MemoryDomain::CpuToGpu,
			.debug_name = ("MaterialUBO_" + std::to_string(i)).c_str(),
		});
	}
}

Material &Material::set(const std::string &param_name, F32 v) {
	if (auto *p = find_parameter(param_name); (p != nullptr) && p->ubo_offset != UINT32_MAX) {
		p->value = v;
		write_ubo(p->ubo_offset, v);
		m_dirty_slot_mask = (1u << SharedConstants::MAX_FRAMES_IN_FLIGHT) - 1u;
	}
	return *this;
}

Material &Material::set(const std::string &param_name, int v) {
	if (auto *p = find_parameter(param_name); (p != nullptr) && p->ubo_offset != UINT32_MAX) {
		p->value = v;
		write_ubo(p->ubo_offset, v);
		m_dirty_slot_mask = (1u << SharedConstants::MAX_FRAMES_IN_FLIGHT) - 1u;
	}
	return *this;
}

Material &Material::set(const std::string &param_name, bool v) {
	if (auto *p = find_parameter(param_name); (p != nullptr) && p->ubo_offset != UINT32_MAX) {
		p->value = v;
		int as_int = v ? 1 : 0;
		write_ubo(p->ubo_offset, as_int);
		m_dirty_slot_mask = (1u << SharedConstants::MAX_FRAMES_IN_FLIGHT) - 1u;
	}
	return *this;
}

Material &Material::set(const std::string &param_name, const Vec2 &v) {
	if (auto *p = find_parameter(param_name); (p != nullptr) && p->ubo_offset != UINT32_MAX) {
		p->value = v;
		write_ubo(p->ubo_offset, v);
		m_dirty_slot_mask = (1u << SharedConstants::MAX_FRAMES_IN_FLIGHT) - 1u;
	}
	return *this;
}

Material &Material::set(const std::string &param_name, const Vec3 &v) {
	if (auto *p = find_parameter(param_name); (p != nullptr) && p->ubo_offset != UINT32_MAX) {
		p->value = v;
		// Only copy 12 bytes; the 4-byte pad is left as zero.
		if (p->ubo_offset + value_size(ParameterType::Vec3) <= m_ubo_data.size()) {
			std::memcpy(m_ubo_data.data() + p->ubo_offset, &v, value_size(ParameterType::Vec3));
		}
		m_dirty_slot_mask = (1u << SharedConstants::MAX_FRAMES_IN_FLIGHT) - 1u;
	}
	return *this;
}

Material &Material::set(const std::string &param_name, const Vec4 &v) {
	if (auto *p = find_parameter(param_name); (p != nullptr) && p->ubo_offset != UINT32_MAX) {
		p->value = v;
		write_ubo(p->ubo_offset, v);
		m_dirty_slot_mask = (1u << SharedConstants::MAX_FRAMES_IN_FLIGHT) - 1u;
	}
	return *this;
}

Material &Material::set(const std::string &param_name, Ref<GFX::GfxTexture> tex) {
	if (auto *p = find_parameter(param_name); (p != nullptr) && p->texture_binding != UINT32_MAX && tex) {
		p->value = tex;
		m_pending_textures.push_back({ p->texture_binding, tex.get() });
	}
	return *this;
}

Material &Material::set_texture(Uint32 binding, GFX::GfxTexture &tex) {
	for (auto &tb : m_pending_textures) {
		if (tb.binding == binding) {
			tb.tex = &tex;
			m_dirty_slot_mask = (1u << SharedConstants::MAX_FRAMES_IN_FLIGHT) - 1u;
			return *this;
		}
	}
	m_pending_textures.push_back({ binding, &tex });
	m_dirty_slot_mask = (1u << SharedConstants::MAX_FRAMES_IN_FLIGHT) - 1u;
	return *this;
}

void Material::flush(Uint32 frame_slot) {
	if (!m_sets[frame_slot]) {
		return;
	}

	const Uint32 slot_bit = 1u << frame_slot;
	if ((m_dirty_slot_mask & slot_bit) == 0) {
		return;
	}

	if (!m_ubo_data.empty()) {
		ensure_uniform_buffers();
		if (m_uniform_buffers[frame_slot]) {
			m_uniform_buffers[frame_slot]->write(m_ubo_data.data(), static_cast<Uint32>(m_ubo_data.size()));
			m_sets[frame_slot]->set_buffer(0, *m_uniform_buffers[frame_slot]);
		}
	}

	for (auto &tb : m_pending_textures) {
		m_sets[frame_slot]->set_texture(tb.binding, *tb.tex);
	}

	m_sets[frame_slot]->flush();

	m_dirty_slot_mask &= ~slot_bit;
	if (m_dirty_slot_mask == 0) {
		m_pending_textures.clear();
	}
}

void Material::bind(GFX::GfxCommandList &cmd, Uint32 set_index, Uint32 frame_slot) {
	cmd.bind_pipeline(*m_pipeline);
	if (m_sets[frame_slot]) {
		cmd.bind_descriptor_set(set_index, *m_sets[frame_slot]);
	}
}

void Material::replace_pipeline(Ref<GFX::GfxPipeline> new_pipeline, Ref<GFX::GfxDescriptorSetLayout> new_layout) {
	m_pipeline = std::move(new_pipeline);

	if (new_layout && (m_context != nullptr)) {
		m_layout = std::move(new_layout);
		for (Uint32 i = 0; i < SharedConstants::MAX_FRAMES_IN_FLIGHT; ++i) {
			m_sets[i] = m_context->allocate_descriptor_set(*m_layout);
		}
		if (m_uniform_buffers[0]) {
			m_dirty_slot_mask = (1u << SharedConstants::MAX_FRAMES_IN_FLIGHT) - 1u;
		}
	}
}

} // namespace Aquila::Graphics
