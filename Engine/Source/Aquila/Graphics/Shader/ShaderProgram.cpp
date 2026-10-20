#include "Aquila/Graphics/Shader/ShaderProgram.h"
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

void ShaderProgram::store_stages(std::vector<RHI::VulkanCompiledStage> &compiled) {
	m_stages.clear();
	for (auto &stage : compiled) {
		ShaderStage s;
		s.stage = vk_stage_to_rhi(stage.stage);
		s.spirv = std::move(stage.spirv);
		s.entry_point_name = std::move(stage.entry_point_name);
		s.linked_component = std::move(stage.linked_component);
		s.session = std::move(stage.session);
		m_stages.push_back(std::move(s));
	}
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
	return compile_from_provider(error_log);
}

bool ShaderProgram::compile_from_provider(std::string &error_log) {
	ShaderSource source;
	if (!m_provider(source, error_log)) {
		return false;
	}
	m_slang_path = source.name;
	m_watch_path = source.watch_path;

	std::vector<RHI::VulkanCompiledStage> compiled;
	if (!RHI::VulkanShaderCompiler::compile_source(source.name, source.code, compiled, error_log,
												   source.search_paths)) {
		return false;
	}
	m_domain = source.domain;
	store_stages(compiled);
	return true;
}

bool ShaderProgram::reload(std::string &error_log, Ref<GFX::GfxDescriptorSetLayout> &out_new_layout) {
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
								   std::map<Uint32, std::map<Uint32, BindingInfo>> &sets) {
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

	auto &bi = sets[set][binding_index];
	bi.descriptor_type = desc_type;
	bi.stage_flags = bi.stage_flags | stage_flags;
	bi.descriptor_count = 1;
	bi.occupied = true;

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

	m_reflected_bindings.push_back(std::move(rb));
}

bool ShaderProgram::reflect_into(Ref<GFX::GfxDescriptorSetLayout> &out_layout) {
	if (m_stages.empty()) {
		return false;
	}

	m_reflected_bindings.clear();
	std::map<Uint32, std::map<Uint32, BindingInfo>> sets;

	const auto &primary = m_stages[0];
	if (primary.linked_component == nullptr) {
		out_layout = nullptr;
		return true;
	}

	slang::ProgramLayout *pl = primary.linked_component->getLayout();
	if (pl == nullptr) {
		out_layout = nullptr;
		return true;
	}

	for (Uint32 i = 0; i < (Uint32)pl->getParameterCount(); ++i) {
		process_binding(pl->getParameterByIndex(i), primary.stage, sets);
	}

	for (size_t si = 1; si < m_stages.size(); ++si) {
		const auto &stage = m_stages[si];
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
			if ((sets.count(s) != 0u) && (sets[s].count(b) != 0u)) {
				sets[s][b].stage_flags = sets[s][b].stage_flags | stage.stage;
			}
		}
	}

	auto it = sets.find(1);
	if (it == sets.end()) {
		out_layout = nullptr;
		return true;
	}

	RHI::DescriptorSetLayoutDesc desc{};
	for (auto &[idx, info] : it->second) {
		if (!info.occupied) {
			continue;
		}
		desc.bindings.push_back({
			.binding = idx,
			.type = info.descriptor_type,
			.stages = info.stage_flags,
			.count = info.descriptor_count,
		});
	}

	out_layout = m_context.create_descriptor_set_layout(desc);
	return true;
}

} // namespace Aquila::Graphics::Shader
