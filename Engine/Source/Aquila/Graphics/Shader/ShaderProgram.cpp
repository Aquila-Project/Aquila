#include "Aquila/Graphics/Shader/ShaderProgram.h"
#include "Aquila/RHI/ShaderPack.h"
#include "Aquila/RHI/SpirvInspection.h"
#include "Aquila/RHI/Vulkan/VulkanShaderCompiler.h"
#include "Aquila/GFX/GfxContext.h"
#include "Aquila/Foundation/Macros.h"
#include <slang/slang.h>

namespace Aquila::Graphics::Shader {

static RHI::ShaderStageFlags vk_stage_to_rhi(VkShaderStageFlagBits stage) {
	switch (stage) {
	case VK_SHADER_STAGE_VERTEX_BIT:
		return RHI::ShaderStageFlags::Vertex;
	case VK_SHADER_STAGE_FRAGMENT_BIT:
		return RHI::ShaderStageFlags::Fragment;
	case VK_SHADER_STAGE_COMPUTE_BIT:
		return RHI::ShaderStageFlags::Compute;
	case VK_SHADER_STAGE_GEOMETRY_BIT:
		return RHI::ShaderStageFlags::Geometry;
	default:
		return RHI::ShaderStageFlags::Vertex;
	}
}

static Option<Graphics::ParameterType> slang_scalar_to_param_type(slang::TypeReflection *type) {
	using Kind = slang::TypeReflection::Kind;
	using Scalar = slang::TypeReflection::ScalarType;

	Kind kind = type->getKind();
	if (kind != Kind::Scalar && kind != Kind::Vector) {
		return std::nullopt;
	}

	const auto cols = kind == Kind::Scalar ? 1U : type->getColumnCount();
	switch (type->getScalarType()) {
	case Scalar::Float32:
		if (cols == 1) {
			return Graphics::ParameterType::Float;
		}
		if (cols == 2) {
			return Graphics::ParameterType::Vec2;
		}
		if (cols == 3) {
			return Graphics::ParameterType::Vec3;
		}
		if (cols == 4) {
			return Graphics::ParameterType::Vec4;
		}
		break;
	case Scalar::Int32:
	case Scalar::UInt32:
		return cols == 1 ? Option<Graphics::ParameterType>(Graphics::ParameterType::Int) : std::nullopt;
	case Scalar::Bool:
		return cols == 1 ? Option<Graphics::ParameterType>(Graphics::ParameterType::Bool) : std::nullopt;
	default:
		break;
	}
	return std::nullopt;
}

std::vector<ShaderStage> ShaderProgram::make_stages(std::vector<RHI::VulkanCompiledStage> &compiled) {
	std::vector<ShaderStage> stages;
	for (auto &stage : compiled) {
		ShaderStage s;
		s.stage = vk_stage_to_rhi(stage.stage);
		s.spirv = std::move(stage.spirv);
		s.entry_point_name = std::move(stage.entry_point_name);
		s.linked_component = std::move(stage.linked_component);
		s.session = std::move(stage.session);
		stages.push_back(std::move(s));
	}
	return stages;
}

void ShaderProgram::store_stages(std::vector<RHI::VulkanCompiledStage> &compiled) {
	m_stages = make_stages(compiled);
}

bool ShaderProgram::add_stage_from_slang(const std::string &slang_path, std::string &error_log) {
	m_slang_path = slang_path;
	m_watch_path = slang_path;

	std::vector<RHI::VulkanCompiledStage> compiled;
	if (!RHI::VulkanShaderCompiler::compile_file(slang_path, compiled, error_log)) {
		return false;
	}
	store_stages(compiled);
	return true;
}

bool ShaderProgram::add_stages_from_provider(SourceProvider provider, std::string &error_log) {
	m_provider = std::move(provider);
	m_precompiled = false;
	return compile_from_provider(error_log);
}

bool ShaderProgram::load_material(const std::string &shader_path, ShaderVariant variant, std::string &error_log) {
	const std::string key = RHI::shader_key(shader_path, variant_name(variant));
	if (const RHI::ShaderPackEntry *entry = RHI::ShaderPacks::find(key)) {
		ShaderDomain domain = ShaderDomain::Custom;
		std::vector<ReflectedBinding> bindings;
		if (!read_metadata(entry->metadata, domain, bindings)) {
			error_log = "The precompiled shader '" + key + "' has no material data";
			return false;
		}
		std::vector<RHI::VulkanCompiledStage> compiled;
		RHI::VulkanShaderCompiler::from_pack(*entry, compiled);
		store_stages(compiled);
		m_provider = {};
		m_slang_path = shader_path;
		m_watch_path.clear();
		m_domain = domain;
		m_reflected_bindings = std::move(bindings);
		m_precompiled = true;
		return true;
	}

	return add_stages_from_provider(
		[shader_path, variant](ShaderSource &out, std::string &error) {
			return load_shader_source(shader_path, out, error, variant);
		},
		error_log);
}

bool ShaderProgram::compile_from_provider(std::string &error_log) {
	ShaderSource source;
	if (!m_provider(source, error_log)) {
		return false;
	}
	m_slang_path = source.name;
	m_watch_path = source.watch_path;

	std::vector<RHI::VulkanCompiledStage> compiled;
	if (!RHI::VulkanShaderCompiler::compile_source(source.name, source.code, compiled, error_log, source.search_paths,
												   source.entry_module)) {
		return false;
	}
	m_domain = source.domain;
	store_stages(compiled);
	return true;
}

bool ShaderProgram::reload(std::string &error_log, Ref<GFX::GfxDescriptorSetLayout> &out_new_layout) {
	if (m_precompiled) {
		error_log = "'" + m_slang_path + "' is precompiled, there is nothing to reload";
		return false;
	}
	if (m_provider) {
		if (!compile_from_provider(error_log)) {
			return false;
		}
		return reflect_into(out_new_layout);
	}

	if (m_slang_path.empty()) {
		error_log = "ShaderProgram: no source path set";
		return false;
	}

	std::vector<RHI::VulkanCompiledStage> compiled;
	if (!RHI::VulkanShaderCompiler::compile_file(m_slang_path, compiled, error_log)) {
		return false;
	}
	store_stages(compiled);
	return reflect_into(out_new_layout);
}

void ShaderProgram::commit_new_layout(Ref<GFX::GfxDescriptorSetLayout> new_layout) {
	m_descriptor_set_layout = std::move(new_layout);
}

bool ShaderProgram::reflect() {
	return reflect_into(m_descriptor_set_layout);
}

void ShaderProgram::cleanup() {
	m_stages.clear();
	m_reflected_bindings.clear();
	m_descriptor_set_layout.reset();
}

bool ShaderProgram::reads_frame_time() const {
	return std::ranges::any_of(
		m_stages, [](const ShaderStage &stage) { return RHI::spirv_reads_member(stage.spirv, "FrameData", "time"); });
}

RHI::ShaderStageDesc ShaderProgram::get_stage_desc(RHI::ShaderStageFlags stage) const {
	for (const auto &s : m_stages) {
		if (s.stage == stage) {
			return { .stage = s.stage, .spirv = s.spirv, .entry_point = s.entry_point_name };
		}
	}
	return {};
}

bool ShaderProgram::slang_type_to_descriptor(slang::TypeLayoutReflection *type_layout, RHI::DescriptorType &out_type,
										  ReflectedBindingType &out_reflected_type) {
	if (type_layout == nullptr) {
		return false;
	}
	slang::TypeReflection *type = type_layout->getType();
	if (type == nullptr) {
		return false;
	}

	using Kind = slang::TypeReflection::Kind;
	switch (type->getKind()) {
	case Kind::ConstantBuffer:
	case Kind::ParameterBlock:
		out_type = RHI::DescriptorType::UniformBuffer;
		out_reflected_type = ReflectedBindingType::UniformBuffer;
		return true;

	case Kind::Resource: {
		SlangResourceShape base = (SlangResourceShape)(type->getResourceShape() & SLANG_RESOURCE_BASE_SHAPE_MASK);

		if (base == SLANG_TEXTURE_1D || base == SLANG_TEXTURE_2D || base == SLANG_TEXTURE_3D ||
			base == SLANG_TEXTURE_CUBE) {
			out_type = RHI::DescriptorType::CombinedImageSampler;
			out_reflected_type = ReflectedBindingType::CombinedImageSampler;
			return true;
		}
		if (base == SLANG_STRUCTURED_BUFFER || base == SLANG_BYTE_ADDRESS_BUFFER) {
			out_type = RHI::DescriptorType::StorageBuffer;
			out_reflected_type = ReflectedBindingType::StorageBuffer;
			return true;
		}
		return false;
	}

	case Kind::SamplerState:

		return false;

	default:
		return false;
	}
}

void ShaderProgram::process_binding(slang::VariableLayoutReflection *var, RHI::ShaderStageFlags stage_flags,
									std::vector<ReflectedBinding> &out) {
	if (var == nullptr) {
		return;
	}

	Uint32 binding_index = (Uint32)var->getBindingIndex();
	Uint32 set = (Uint32)var->getBindingSpace();

	slang::TypeLayoutReflection *type_layout = var->getTypeLayout();
	if (type_layout == nullptr) {
		return;
	}

	RHI::DescriptorType desc_type{};
	ReflectedBindingType refl_type{};
	if (!slang_type_to_descriptor(type_layout, desc_type, refl_type)) {
		return;
	}

	ReflectedBinding rb{};
	rb.name = (var->getName() != nullptr) ? var->getName() : "";
	rb.set = set;
	rb.binding_index = binding_index;
	rb.descriptor_count = 1;
	rb.type = refl_type;
	rb.stage_flags = stage_flags;

	if (refl_type == ReflectedBindingType::UniformBuffer) {
		slang::TypeLayoutReflection *inner = type_layout->getElementTypeLayout();
		if (inner != nullptr) {
			rb.ubo_size = static_cast<Uint32>(inner->getSize());
			for (Uint32 f = 0; f < (Uint32)inner->getFieldCount(); ++f) {
				slang::VariableLayoutReflection *field = inner->getFieldByIndex(f);
				if ((field == nullptr) || (field->getName() == nullptr)) {
					continue;
				}
				slang::TypeReflection *ft = (field->getTypeLayout() != nullptr) ? field->getTypeLayout()->getType() : nullptr;
				if (ft == nullptr) {
					continue;
				}
				ReflectedBinding::UBOField uf{};
				uf.name = field->getName();
				uf.offset = static_cast<Uint32>(field->getOffset());
				const Option<Graphics::ParameterType> param_type = slang_scalar_to_param_type(ft);
				uf.supported = param_type.has_value();
				uf.param_type = param_type.value_or(Graphics::ParameterType::Float);
				rb.ubo_fields.push_back(std::move(uf));
			}
		}
	}

	out.push_back(std::move(rb));
}

std::vector<ShaderProgram::ReflectedBinding> ShaderProgram::reflect_stages(const std::vector<ShaderStage> &stages) {
	std::vector<ReflectedBinding> out;
	if (stages.empty() || stages[0].linked_component == nullptr) {
		return out;
	}

	const auto &primary = stages[0];
	slang::ProgramLayout *pl = primary.linked_component->getLayout();
	if (pl == nullptr) {
		return out;
	}

	for (Uint32 i = 0; i < (Uint32)pl->getParameterCount(); ++i) {
		process_binding(pl->getParameterByIndex(i), primary.stage, out);
	}

	for (size_t si = 1; si < stages.size(); ++si) {
		const auto &stage = stages[si];
		if (stage.linked_component == nullptr) {
			continue;
		}
		slang::ProgramLayout *spl = stage.linked_component->getLayout();
		if (spl == nullptr) {
			continue;
		}
		for (Uint32 i = 0; i < (Uint32)spl->getParameterCount(); ++i) {
			auto *var = spl->getParameterByIndex(i);
			if (var == nullptr) {
				continue;
			}
			Uint32 b = (Uint32)var->getBindingIndex();
			Uint32 s = (Uint32)var->getBindingSpace();
			for (ReflectedBinding &binding : out) {
				if (binding.set == s && binding.binding_index == b) {
					binding.stage_flags = binding.stage_flags | stage.stage;
				}
			}
		}
	}
	return out;
}

bool ShaderProgram::reflect_into(Ref<GFX::GfxDescriptorSetLayout> &out_layout) {
	if (m_stages.empty()) {
		return false;
	}

	if (!m_precompiled) {
		m_reflected_bindings = reflect_stages(m_stages);
	}

	std::map<Uint32, RHI::DescriptorBinding> set_one;
	for (const ReflectedBinding &binding : m_reflected_bindings) {
		if (binding.set != 1) {
			continue;
		}
		RHI::DescriptorType type = RHI::DescriptorType::UniformBuffer;
		switch (binding.type) {
		case ReflectedBindingType::CombinedImageSampler:
			type = RHI::DescriptorType::CombinedImageSampler;
			break;
		case ReflectedBindingType::StorageBuffer:
			type = RHI::DescriptorType::StorageBuffer;
			break;
		case ReflectedBindingType::UniformBuffer:
		case ReflectedBindingType::Unknown:
			break;
		}
		auto [it, inserted] = set_one.try_emplace(binding.binding_index,
												  RHI::DescriptorBinding{
													  .binding = binding.binding_index,
													  .type = type,
													  .stages = binding.stage_flags,
													  .count = binding.descriptor_count,
												  });
		if (!inserted) {
			it->second.type = type;
			it->second.stages = it->second.stages | binding.stage_flags;
		}
	}

	if (set_one.empty()) {
		out_layout = nullptr;
		return true;
	}

	RHI::DescriptorSetLayoutDesc desc{};
	for (const auto &[index, binding] : set_one) {
		desc.bindings.push_back(binding);
	}
	out_layout = m_context.create_descriptor_set_layout(desc);
	return true;
}

namespace {

class MetadataWriter {
  public:
	void u8(Uint8 value) { m_data.push_back(value); }

	void u32(Uint32 value) {
		for (Uint32 i = 0; i < 4; ++i) {
			m_data.push_back(static_cast<Uint8>(value >> (i * 8)));
		}
	}

	void string(std::string_view text) {
		u32(static_cast<Uint32>(text.size()));
		m_data.insert(m_data.end(), text.begin(), text.end());
	}

	std::vector<Uint8> take() { return std::move(m_data); }

  private:
	std::vector<Uint8> m_data;
};

class MetadataReader {
  public:
	explicit MetadataReader(std::span<const Uint8> data) : m_data(data) {}

	bool u8(Uint8 &out) {
		if (m_offset >= m_data.size()) {
			return false;
		}
		out = m_data[m_offset++];
		return true;
	}

	bool u32(Uint32 &out) {
		if (m_data.size() - m_offset < 4) {
			return false;
		}
		out = 0;
		for (Uint32 i = 0; i < 4; ++i) {
			out |= static_cast<Uint32>(m_data[m_offset++]) << (i * 8);
		}
		return true;
	}

	bool string(std::string &out) {
		Uint32 size = 0;
		if (!u32(size) || size > m_data.size() - m_offset) {
			return false;
		}
		out.assign(reinterpret_cast<const char *>(m_data.data() + m_offset), size);
		m_offset += size;
		return true;
	}

  private:
	std::span<const Uint8> m_data;
	Usize m_offset = 0;
};

constexpr Uint8 K_METADATA_VERSION = 1;

}

std::vector<Uint8> ShaderProgram::write_metadata(ShaderDomain domain, const std::vector<ReflectedBinding> &bindings) {
	MetadataWriter writer;
	writer.u8(K_METADATA_VERSION);
	writer.u8(static_cast<Uint8>(domain));
	writer.u32(static_cast<Uint32>(bindings.size()));
	for (const ReflectedBinding &binding : bindings) {
		writer.string(binding.name);
		writer.u32(binding.set);
		writer.u32(binding.binding_index);
		writer.u32(binding.descriptor_count);
		writer.u8(static_cast<Uint8>(binding.type));
		writer.u32(static_cast<Uint32>(binding.stage_flags));
		writer.u32(binding.ubo_size);
		writer.u32(static_cast<Uint32>(binding.ubo_fields.size()));
		for (const ReflectedBinding::UBOField &field : binding.ubo_fields) {
			writer.string(field.name);
			writer.u8(static_cast<Uint8>(field.param_type));
			writer.u32(field.offset);
			writer.u8(field.supported ? 1 : 0);
		}
	}
	return writer.take();
}

bool ShaderProgram::read_metadata(std::span<const Uint8> data, ShaderDomain &domain,
								  std::vector<ReflectedBinding> &bindings) {
	MetadataReader reader(data);
	Uint8 version = 0;
	Uint8 domain_value = 0;
	Uint32 count = 0;
	if (!reader.u8(version) || version != K_METADATA_VERSION || !reader.u8(domain_value) ||
		domain_value > static_cast<Uint8>(ShaderDomain::Custom) || !reader.u32(count)) {
		return false;
	}
	domain = static_cast<ShaderDomain>(domain_value);

	bindings.clear();
	for (Uint32 i = 0; i < count; ++i) {
		ReflectedBinding binding{};
		Uint8 type = 0;
		Uint32 stages = 0;
		Uint32 field_count = 0;
		if (!reader.string(binding.name) || !reader.u32(binding.set) || !reader.u32(binding.binding_index) ||
			!reader.u32(binding.descriptor_count) || !reader.u8(type) || !reader.u32(stages) ||
			!reader.u32(binding.ubo_size) || !reader.u32(field_count)) {
			return false;
		}
		binding.type = static_cast<ReflectedBindingType>(type);
		binding.stage_flags = static_cast<RHI::ShaderStageFlags>(stages);
		for (Uint32 f = 0; f < field_count; ++f) {
			ReflectedBinding::UBOField field;
			Uint8 param_type = 0;
			Uint8 supported = 0;
			if (!reader.string(field.name) || !reader.u8(param_type) || !reader.u32(field.offset) ||
				!reader.u8(supported)) {
				return false;
			}
			field.param_type = static_cast<Graphics::ParameterType>(param_type);
			field.supported = supported != 0;
			binding.ubo_fields.push_back(std::move(field));
		}
		bindings.push_back(std::move(binding));
	}
	return true;
}

} // namespace Aquila::Graphics::Shader
