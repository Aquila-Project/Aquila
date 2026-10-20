#include "Aquila/Graphics/Material/Material.h"
#include "Aquila/Graphics/Shader/ShaderProgram.h"
#include "Aquila/Graphics/Texture/TextureLibrary.h"
#include "Aquila/GFX/GfxContext.h"
#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/Foundation/SharedConstants.h"

namespace Aquila::Graphics {

namespace {

constexpr Uint32 k_all_slots = (1U << SharedConstants::MAX_FRAMES_IN_FLIGHT) - 1U;

bool name_has(std::string_view name, std::string_view word) {
	if (word.size() > name.size()) {
		return false;
	}
	for (Usize i = 0; i + word.size() <= name.size(); ++i) {
		bool match = true;
		for (Usize j = 0; j < word.size(); ++j) {
			if (std::tolower(static_cast<unsigned char>(name[i + j])) !=
				std::tolower(static_cast<unsigned char>(word[j]))) {
				match = false;
				break;
			}
		}
		if (match) {
			return true;
		}
	}
	return false;
}

bool is_color_name(std::string_view name) {
	return name_has(name, "color") || name_has(name, "colour") || name_has(name, "tint") || name_has(name, "albedo");
}

DefaultTexture loading_placeholder(const MaterialParameter &param) {
	if (param.default_texture == DefaultTexture::FlatNormal) {
		return DefaultTexture::FlatNormal;
	}
	if (name_has(param.name, "emissive") || name_has(param.name, "emission")) {
		return DefaultTexture::Black;
	}
	return param.srgb ? DefaultTexture::Gray : DefaultTexture::White;
}

bool is_engine_input(std::string_view name) {
	return name == "sceneColor" || name == "sceneDepth";
}

ParameterValue default_value(const std::string &name, ParameterType t) {
	switch (t) {
	case ParameterType::Float:
		if (name_has(name, "roughness")) {
			return 0.5F;
		}
		if (name_has(name, "strength") || name_has(name, "scale") || name_has(name, "opacity")) {
			return 1.F;
		}
		return 0.F;
	case ParameterType::Int:
		return 0;
	case ParameterType::Bool:
		return false;
	case ParameterType::Vec2:
		return name_has(name, "scale") || name_has(name, "tiling") ? Vec2{ 1.F, 1.F } : Vec2{ 0.F, 0.F };
	case ParameterType::Vec3:
		return is_color_name(name) ? Vec3{ 1.F, 1.F, 1.F } : Vec3{ 0.F, 0.F, 0.F };
	case ParameterType::Vec4:
	case ParameterType::Color:
		return is_color_name(name) ? Vec4{ 1.F, 1.F, 1.F, 1.F } : Vec4{ 0.F, 0.F, 0.F, 0.F };
	default:
		return 0.F;
	}
}

F32 as_float(const MaterialValue &value) {
	if (const auto *f = std::get_if<F32>(&value)) {
		return *f;
	}
	if (const auto *i = std::get_if<Int32>(&value)) {
		return static_cast<F32>(*i);
	}
	if (const auto *b = std::get_if<bool>(&value)) {
		return *b ? 1.F : 0.F;
	}
	return 0.F;
}

Vec4 as_vec4(const MaterialValue &value, F32 w) {
	if (const auto *v = std::get_if<Vec4>(&value)) {
		return *v;
	}
	if (const auto *v = std::get_if<Vec3>(&value)) {
		return Vec4(*v, w);
	}
	if (const auto *v = std::get_if<Vec2>(&value)) {
		return Vec4(v->x, v->y, 0.F, w);
	}
	return Vec4(as_float(value));
}

}

Ref<Material> Material::create_from_shader(GFX::GfxContext &ctx, Shader::ShaderProgram &shader,
										   Ref<GFX::GfxPipeline> pipeline) {
	auto mat = Ref<Material>(new Material());
	mat->m_context = &ctx;
	mat->m_pipeline = std::move(pipeline);
	mat->m_domain = shader.get_domain();
	mat->build_parameters(shader);
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

void Material::build_parameters(Shader::ShaderProgram &shader) {
	using RBType = Shader::ShaderProgram::ReflectedBindingType;

	m_layout = shader.m_descriptor_set_layout;
	m_sets = {};
	m_uniform_buffers = {};
	m_parameters.clear();
	m_ubo_data.clear();
	m_textures.clear();
	m_ubo_binding = 0;

	if (m_layout) {
		for (Uint32 i = 0; i < SharedConstants::MAX_FRAMES_IN_FLIGHT; ++i) {
			m_sets[i] = m_context->allocate_descriptor_set(*m_layout);
		}
	}

	Uint32 ubo_size = 0;
	for (const auto &binding : shader.get_reflected_bindings()) {
		if (binding.set != 1) {
			continue;
		}
		if (binding.type == RBType::UniformBuffer) {
			m_ubo_binding = binding.binding_index;
			ubo_size = std::max(ubo_size, binding.ubo_size);
			for (const auto &field : binding.ubo_fields) {
				if (!field.supported) {
					continue;
				}
				MaterialParameter p;
				p.name = field.name;
				p.type = field.param_type;
				p.ubo_offset = field.offset;
				p.default_value = default_value(field.name, field.param_type);
				p.value = p.default_value;
				m_parameters.push_back(std::move(p));
			}
		} else if (binding.type == RBType::CombinedImageSampler) {
			MaterialParameter p;
			p.name = binding.name;
			p.type = ParameterType::Texture2D;
			p.texture_binding = binding.binding_index;
			p.hidden = is_engine_input(binding.name);
			p.srgb = is_srgb_texture_name(binding.name);
			p.default_texture = name_has(binding.name, "normal") ? DefaultTexture::FlatNormal : DefaultTexture::White;
			m_parameters.push_back(std::move(p));
		}
	}

	ubo_size = (ubo_size + 15U) & ~15U;
	if (ubo_size > 0 && m_sets[0]) {
		m_ubo_data.assign(ubo_size, 0);
		for (Uint32 i = 0; i < SharedConstants::MAX_FRAMES_IN_FLIGHT; ++i) {
			m_uniform_buffers[i] = m_context->create_buffer({
				.size = ubo_size,
				.usage = RHI::BufferUsage::UniformBuffer,
				.domain = RHI::MemoryDomain::CpuToGpu,
				.debug_name = "MaterialUBO_" + std::to_string(i),
			});
		}
	}

	for (MaterialParameter &p : m_parameters) {
		if (p.type == ParameterType::Texture2D) {
			bind_texture(p, nullptr);
		} else {
			write_parameter(p, get_default_value(p));
		}
	}

	const MaterialData data = m_data;
	apply(data);
	m_texture_revision = TextureLibrary::get()->get_revision();
	mark_dirty();
}

void Material::rebuild(Shader::ShaderProgram &shader, Ref<GFX::GfxPipeline> pipeline) {
	m_pipeline = std::move(pipeline);
	m_domain = shader.get_domain();
	build_parameters(shader);
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
	p.value = default_value(p.name, type);
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

MaterialValue Material::get_default_value(const MaterialParameter &param) {
	return std::visit(
		[](const auto &stored) -> MaterialValue {
			using V = std::decay_t<decltype(stored)>;
			if constexpr (std::is_same_v<V, Ref<GFX::GfxTexture>>) {
				return TextureReference{};
			} else if constexpr (std::is_same_v<V, int>) {
				return static_cast<Int32>(stored);
			} else {
				return stored;
			}
		},
		param.default_value);
}

MaterialValue Material::get_value(const MaterialParameter &param) const {
	const auto it = m_data.values.find(param.name);
	if (it == m_data.values.end()) {
		return get_default_value(param);
	}
	switch (param.type) {
	case ParameterType::Float:
		return as_float(it->second);
	case ParameterType::Int:
		return static_cast<Int32>(std::lround(as_float(it->second)));
	case ParameterType::Bool:
		return as_float(it->second) != 0.F;
	case ParameterType::Vec2: {
		const Vec4 v = as_vec4(it->second, 0.F);
		return Vec2(v.x, v.y);
	}
	case ParameterType::Vec3:
		return Vec3(as_vec4(it->second, 1.F));
	case ParameterType::Vec4:
	case ParameterType::Color:
		return as_vec4(it->second, 1.F);
	case ParameterType::Texture2D:
		if (const auto *texture = std::get_if<TextureReference>(&it->second)) {
			return *texture;
		}
		return TextureReference{};
	}
	return it->second;
}

void Material::set_value(const std::string &param_name, const MaterialValue &value) {
	MaterialParameter *p = find_parameter(param_name);
	if (p == nullptr) {
		m_data.values[param_name] = value;
		return;
	}
	if (p->hidden) {
		return;
	}
	m_data.values[param_name] = value;
	const MaterialValue normalized = get_value(*p);
	if (normalized == get_default_value(*p)) {
		m_data.values.erase(param_name);
	} else {
		m_data.values[param_name] = normalized;
	}
	write_parameter(*p, normalized);
}


void Material::apply(const MaterialData &data) {
	m_data = data;
	for (MaterialParameter &p : m_parameters) {
		if (p.hidden) {
			continue;
		}
		write_parameter(p, get_value(p));
	}
}

void Material::write_parameter(MaterialParameter &param, const MaterialValue &value) {
	switch (param.type) {
	case ParameterType::Texture2D: {
		const auto *reference = std::get_if<TextureReference>(&value);
		const bool has_path = reference != nullptr && !reference->path.empty() && m_context != nullptr;
		Ref<GFX::GfxTexture> texture =
			has_path ? TextureLibrary::get()->load(*m_context, reference->path, param.srgb) : nullptr;
		const bool loading = has_path && !texture && TextureLibrary::get()->is_loading(reference->path, param.srgb);
		bind_texture(param, std::move(texture), loading);
		return;
	}
	case ParameterType::Float:
		set(param.name, as_float(value));
		return;
	case ParameterType::Int:
		set(param.name, static_cast<int>(std::lround(as_float(value))));
		return;
	case ParameterType::Bool:
		set(param.name, as_float(value) != 0.F);
		return;
	case ParameterType::Vec2: {
		const Vec4 v = as_vec4(value, 0.F);
		set(param.name, Vec2(v.x, v.y));
		return;
	}
	case ParameterType::Vec3:
		set(param.name, Vec3(as_vec4(value, 1.F)));
		return;
	case ParameterType::Vec4:
	case ParameterType::Color:
		set(param.name, as_vec4(value, 1.F));
		return;
	}
}

void Material::bind_texture(MaterialParameter &param, Ref<GFX::GfxTexture> texture, bool loading) {
	if (param.texture_binding == UINT32_MAX || m_context == nullptr) {
		return;
	}
	const DefaultTexture fallback = loading ? loading_placeholder(param) : param.default_texture;
	GFX::GfxTexture *raw = texture ? texture.get() : &TextureLibrary::get()->get_default(*m_context, fallback);
	param.value = texture;
	m_textures[param.texture_binding] = BoundTexture{ .texture = raw, .owner = std::move(texture) };
	mark_dirty();
}

void Material::mark_dirty() {
	m_dirty_slot_mask = k_all_slots;
}

template <typename T> void Material::write_ubo(Uint32 offset, const T &v) {
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
			.debug_name = "MaterialUBO_" + std::to_string(i),
		});
	}
}

Material &Material::set(const std::string &param_name, F32 v) {
	if (auto *p = find_parameter(param_name); (p != nullptr) && p->ubo_offset != UINT32_MAX) {
		p->value = v;
		write_ubo(p->ubo_offset, v);
		mark_dirty();
	}
	return *this;
}

Material &Material::set(const std::string &param_name, int v) {
	if (auto *p = find_parameter(param_name); (p != nullptr) && p->ubo_offset != UINT32_MAX) {
		p->value = v;
		write_ubo(p->ubo_offset, v);
		mark_dirty();
	}
	return *this;
}

Material &Material::set(const std::string &param_name, bool v) {
	if (auto *p = find_parameter(param_name); (p != nullptr) && p->ubo_offset != UINT32_MAX) {
		p->value = v;
		const int as_int = v ? 1 : 0;
		write_ubo(p->ubo_offset, as_int);
		mark_dirty();
	}
	return *this;
}

Material &Material::set(const std::string &param_name, const Vec2 &v) {
	if (auto *p = find_parameter(param_name); (p != nullptr) && p->ubo_offset != UINT32_MAX) {
		p->value = v;
		write_ubo(p->ubo_offset, v);
		mark_dirty();
	}
	return *this;
}

Material &Material::set(const std::string &param_name, const Vec3 &v) {
	if (auto *p = find_parameter(param_name); (p != nullptr) && p->ubo_offset != UINT32_MAX) {
		p->value = v;
		write_ubo(p->ubo_offset, v);
		mark_dirty();
	}
	return *this;
}

Material &Material::set(const std::string &param_name, const Vec4 &v) {
	if (auto *p = find_parameter(param_name); (p != nullptr) && p->ubo_offset != UINT32_MAX) {
		p->value = v;
		write_ubo(p->ubo_offset, v);
		mark_dirty();
	}
	return *this;
}

Material &Material::set(const std::string &param_name, Ref<GFX::GfxTexture> tex) {
	if (auto *p = find_parameter(param_name); (p != nullptr) && p->texture_binding != UINT32_MAX) {
		bind_texture(*p, std::move(tex));
	}
	return *this;
}

Material &Material::set_texture(Uint32 binding, GFX::GfxTexture &tex) {
	m_textures[binding] = BoundTexture{ .texture = &tex, .owner = nullptr };
	mark_dirty();
	return *this;
}

void Material::refresh_textures() {
	const Uint64 revision = TextureLibrary::get()->get_revision();
	if (revision == m_texture_revision) {
		return;
	}
	m_texture_revision = revision;
	for (MaterialParameter &param : m_parameters) {
		if (param.type != ParameterType::Texture2D || param.hidden) {
			continue;
		}
		const MaterialValue value = get_value(param);
		const auto *reference = std::get_if<TextureReference>(&value);
		if (reference != nullptr && !reference->path.empty()) {
			write_parameter(param, value);
		}
	}
}

void Material::flush(Uint32 frame_slot) {
	if (!m_sets[frame_slot]) {
		return;
	}
	refresh_textures();

	const Uint32 slot_bit = 1U << frame_slot;
	if ((m_dirty_slot_mask & slot_bit) == 0) {
		return;
	}

	if (!m_ubo_data.empty()) {
		ensure_uniform_buffers();
		if (m_uniform_buffers[frame_slot]) {
			m_uniform_buffers[frame_slot]->write(m_ubo_data.data(), static_cast<Uint32>(m_ubo_data.size()));
			m_sets[frame_slot]->set_buffer(m_ubo_binding, *m_uniform_buffers[frame_slot]);
		}
	}

	for (const auto &[binding, bound] : m_textures) {
		if (bound.texture != nullptr) {
			m_sets[frame_slot]->set_texture(binding, *bound.texture);
		}
	}

	m_sets[frame_slot]->flush();
	m_dirty_slot_mask &= ~slot_bit;
}

void Material::bind(GFX::GfxCommandList &cmd, Uint32 set_index, Uint32 frame_slot) {
	cmd.bind_pipeline(*m_pipeline);
	if (m_sets[frame_slot]) {
		cmd.bind_descriptor_set(set_index, *m_sets[frame_slot]);
	}
}

void Material::bind_descriptor_set(GFX::GfxCommandList &cmd, Uint32 set_index, Uint32 frame_slot) {
	if (m_sets[frame_slot]) {
		cmd.bind_descriptor_set(set_index, *m_sets[frame_slot]);
	}
}

void Material::bind_input(Uint32 frame_slot, Uint32 binding, GFX::GfxTexture &texture) {
	if (!m_sets[frame_slot]) {
		return;
	}
	m_sets[frame_slot]->set_texture(binding, texture);
	m_sets[frame_slot]->flush();
}

} // namespace Aquila::Graphics
