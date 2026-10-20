#pragma once
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/Graphics/Material/MaterialData.h"
#include "Aquila/Graphics/Material/MaterialParameters.h"
#include "Aquila/Graphics/Shader/ShaderSource.h"
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

/// A shader's pipeline plus one set of values for its parameters (set 1: the uniform buffer and textures). Every
/// .aqmat is one, so is every entity's own material. Values live in a CPU copy of the uniform buffer, changing one
/// marks every frame slot dirty and flush(slot) uploads to that slot before it's drawn.
///
/// @code
/// mat->set_value("baseColor", Vec4(1.F, 0.2F, 0.2F, 1.F));
/// mat->set_value("baseColorMap", TextureReference{ "/project/Content/brick.png" });
/// mat->flush(frame_slot);
/// mat->bind(cmd, 1, frame_slot);
/// @endcode
///
/// @note The typed set() overloads ignore names the shader doesn't have and don't check the type, set_value()
/// converts and remembers the value in get_data().
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

	/// Keeps a raw pointer, the texture has to outlive the material.
	Material &set_texture(Uint32 binding, GFX::GfxTexture &tex);

	/// Converts `value` to the parameter's type and remembers it in get_data(). A value equal to the default is
	/// forgotten instead. Textures load through TextureLibrary, an empty path binds the default back. Names the
	/// shader doesn't have are still remembered, so they come back if the shader gets them again.
	void set_value(const std::string &param_name, const MaterialValue &value);
	/// Takes `data` (shader, double sided, values) and applies every value the shader has. It doesn't switch the
	/// shader: a different `shader` needs a new material (MaterialAssets does that).
	void apply(const MaterialData &data);
	[[nodiscard]] const MaterialData &get_data() const { return m_data; }
	/// What the parameter shows right now: the material's value or the default.
	[[nodiscard]] MaterialValue get_value(const MaterialParameter &param) const;
	[[nodiscard]] static MaterialValue get_default_value(const MaterialParameter &param);

	/// Uploads the values to the slot when they changed, and first rebinds textures TextureLibrary finished loading
	/// or hot reloaded.
	void flush(Uint32 frame_slot);

	/// Binds the pipeline too.
	void bind(GFX::GfxCommandList &cmd, Uint32 set_index, Uint32 frame_slot);

	[[nodiscard]] GFX::GfxPipeline &get_pipeline() { return *m_pipeline; }
	[[nodiscard]] bool has_descriptor_set() const { return m_sets[0] != nullptr; }
	[[nodiscard]] const std::vector<MaterialParameter> &get_parameters() const { return m_parameters; }
	[[nodiscard]] const MaterialParameter *get_parameter(const std::string &param_name) const;

	[[nodiscard]] MaterialType get_type() const { return m_type; }
	void set_type(MaterialType type) { m_type = type; }
	[[nodiscard]] Shader::ShaderDomain get_domain() const { return m_domain; }
	/// The compiled path: a full engine shader path or a VFS path.
	[[nodiscard]] const std::string &get_shader_path() const { return m_shader_path; }
	/// The .aqmat it was loaded from, empty for an entity's own material.
	[[nodiscard]] const std::string &get_asset_path() const { return m_asset_path; }
	void set_asset_path(std::string path) { m_asset_path = std::move(path); }
	/// True when the shader reads FrameData.time. The renderer keeps requesting frames while it's drawn.
	[[nodiscard]] bool uses_time() const { return m_uses_time; }

  private:
	Material() = default;

	void replace_pipeline(Ref<GFX::GfxPipeline> new_pipeline, Ref<GFX::GfxDescriptorSetLayout> new_layout = nullptr);
	/// Reads the parameters from the shader again (after a hot reload), keeps the values in get_data().
	void rebuild(Shader::ShaderProgram &shader, Ref<GFX::GfxPipeline> pipeline);
	void build_parameters(Shader::ShaderProgram &shader);
	void write_parameter(MaterialParameter &param, const MaterialValue &value);
	void refresh_textures();
	/// A null texture binds the parameter's default, or its loading placeholder when `loading`.
	void bind_texture(MaterialParameter &param, Ref<GFX::GfxTexture> texture, bool loading = false);
	void mark_dirty();

	friend class MaterialFactory;

	struct BoundTexture {
		GFX::GfxTexture *texture = nullptr;
		Ref<GFX::GfxTexture> owner;
	};

	GFX::GfxContext *m_context = nullptr;
	Ref<GFX::GfxPipeline> m_pipeline;
	Uint64 m_texture_revision = 0;
	MaterialType m_type = MaterialType::PBR;
	Shader::ShaderDomain m_domain = Shader::ShaderDomain::Custom;
	std::string m_shader_path;
	std::string m_asset_path;
	bool m_uses_time = false;
	MaterialData m_data;
	Ref<GFX::GfxDescriptorSetLayout> m_layout;

	std::array<Ref<GFX::GfxDescriptorSet>, SharedConstants::MAX_FRAMES_IN_FLIGHT> m_sets;
	std::array<Ref<GFX::GfxBuffer>, SharedConstants::MAX_FRAMES_IN_FLIGHT> m_uniform_buffers;
	Uint32 m_ubo_binding = 0;

	Uint32 m_dirty_slot_mask = 0;

	std::vector<MaterialParameter> m_parameters;
	std::vector<Uint8> m_ubo_data;
	std::map<Uint32, BoundTexture> m_textures;

	MaterialParameter *find_parameter(const std::string &param_name);

	template <typename T> void write_ubo(Uint32 offset, const T &v);

	void ensure_uniform_buffers();
};

} // namespace Aquila::Graphics
