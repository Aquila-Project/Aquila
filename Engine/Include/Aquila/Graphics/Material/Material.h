#pragma once
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/Graphics/Material/MaterialParameters.h"
#include "Aquila/GFX/GfxPipeline.h"
#include "Aquila/GFX/GfxDescriptorSet.h"
#include "Aquila/GFX/GfxBuffer.h"
#include "Aquila/GFX/GfxTexture.h"
#include "Aquila/GFX/GfxCommandList.h"

namespace Aquila::GFX {
class GfxContext;
}

namespace Aquila::Graphics::Shader {
class ShaderProgram;
}

namespace Aquila::Graphics {

enum class MaterialType : Uint8 {
	PBR,
	Lit,
	Unlit,
	Custom,
};

class MaterialFactory;

class Material {
  public:
	std::string name;

	static Ref<Material> create_from_shader(GFX::GfxContext &ctx, Shader::ShaderProgram &shader,
											Ref<GFX::GfxPipeline> pipeline);

	static Ref<Material> create(GFX::GfxContext &ctx, Ref<GFX::GfxPipeline> pipeline,
								Ref<GFX::GfxDescriptorSetLayout> layout = nullptr);

	void register_parameter(std::string param_name, ParameterType type, Uint32 ubo_offset,
							Uint32 texture_binding = UINT32_MAX);

	Material &set(const std::string &param_name, F32 v);
	Material &set(const std::string &param_name, int v);
	Material &set(const std::string &param_name, bool v);
	Material &set(const std::string &param_name, const Vec2 &v);
	Material &set(const std::string &param_name, const Vec3 &v);
	Material &set(const std::string &param_name, const Vec4 &v);
	Material &set(const std::string &param_name, Ref<GFX::GfxTexture> tex);

	Material &set_albedo(const Vec4 &color) { return set("albedo", color); }
	Material &set_albedo(const Vec3 &color) { return set("albedo", Vec4{ color, 1.F }); }
	Material &set_metallic(F32 v) { return set("metallic", v); }
	Material &set_roughness(F32 v) { return set("roughness", v); }
	Material &set_emissive(F32 v) { return set("emissive", v); }

	Material &set_texture(Uint32 binding, GFX::GfxTexture &tex);

	void flush(Uint32 frame_slot);

	void bind(GFX::GfxCommandList &cmd, Uint32 set_index, Uint32 frame_slot);

	[[nodiscard]] GFX::GfxPipeline &get_pipeline() { return *m_pipeline; }
	[[nodiscard]] bool has_descriptor_set() const { return m_sets[0] != nullptr; }
	[[nodiscard]] const std::vector<MaterialParameter> &get_parameters() const { return m_parameters; }
	[[nodiscard]] const MaterialParameter *get_parameter(const std::string &param_name) const;

	[[nodiscard]] MaterialType get_type() const { return m_type; }
	void set_type(MaterialType type) { m_type = type; }
	[[nodiscard]] const std::string &get_shader_path() const { return m_shader_path; }
	[[nodiscard]] bool uses_time() const { return m_uses_time; }

  private:
	Material() = default;

	void replace_pipeline(Ref<GFX::GfxPipeline> new_pipeline, Ref<GFX::GfxDescriptorSetLayout> new_layout = nullptr);

	friend class MaterialFactory;

	GFX::GfxContext *m_context = nullptr;
	Ref<GFX::GfxPipeline> m_pipeline;
	MaterialType m_type = MaterialType::PBR;
	std::string m_shader_path;
	bool m_uses_time = false;
	Ref<GFX::GfxDescriptorSetLayout> m_layout;

	std::array<Ref<GFX::GfxDescriptorSet>, SharedConstants::MAX_FRAMES_IN_FLIGHT> m_sets;
	std::array<Ref<GFX::GfxBuffer>, SharedConstants::MAX_FRAMES_IN_FLIGHT> m_uniform_buffers;

	Uint32 m_dirty_slot_mask = 0;

	std::vector<MaterialParameter> m_parameters;
	std::vector<Uint8> m_ubo_data;

	struct PendingTexture {
		Uint32 binding;
		GFX::GfxTexture *tex;
	};
	std::vector<PendingTexture> m_pending_textures;

	MaterialParameter *find_parameter(const std::string &param_name);

	template <typename T> void write_ubo(Uint32 offset, const T &v);

	void ensure_uniform_buffers();
};

} // namespace Aquila::Graphics
