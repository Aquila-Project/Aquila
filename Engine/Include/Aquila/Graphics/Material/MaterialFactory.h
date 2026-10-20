#pragma once
#include "Aquila/Foundation/Singleton.h"
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Graphics/Material/Material.h"
#include "Aquila/RHI/Backend/RHITypes.h"

namespace Aquila::GFX {
class GfxContext;
class GfxDescriptorSetLayout;
} // namespace Aquila::GFX

namespace Aquila::Graphics::Shader {
class ShaderProgram;
}

namespace Aquila::Graphics {

struct MaterialCreateInfo {
	MaterialType type = MaterialType::PBR;

	std::vector<RHI::TextureFormat> color_formats = { RHI::TextureFormat::RGBA16F };
	RHI::TextureFormat depth_format = RHI::TextureFormat::Depth32;

	RHI::CullMode cull_mode = RHI::CullMode::Back;
	RHI::FrontFace front_face = RHI::FrontFace::Clockwise;

	bool depth_test = true;
	bool depth_write = true;

	bool blend_enabled = false;
	/// A fullscreen triangle: no vertex input, no depth.
	bool fullscreen = false;

	Uint32 push_constant_size = 256;

	bool operator==(const MaterialCreateInfo &) const = default;
};

/// Builds materials from Slang shaders. Every material made from the same path shares one shader program (and one
/// pipeline per render state), and editing the file rebuilds all of them in place, keeping their values.
///
/// @code
/// Ref<Material> mat = MaterialFactory::get()->create(ctx, MaterialData{ .shader = "Basic.slang" });
/// mat->set_value("roughness", 0.4F);
/// @endcode
class MaterialFactory : public Foundation::Singleton<MaterialFactory> {
  public:
	~MaterialFactory();

	/// The scene descriptor layout every material pipeline gets. RenderPipeline sets it,
	/// create() asserts without it.
	void set_scene_layout(GFX::GfxDescriptorSetLayout &layout) { m_scene_layout = &layout; }

	/// Compiles `data.shader` for its domain (see Shader::ShaderDomain), with the render state the domain and
	/// `data.double_sided` ask for, and applies `data`. Null when the shader doesn't compile.
	/// @note A shader that didn't compile isn't compiled again until its sources change (retry_failed()), so asking
	/// for it every frame is cheap and logs the error once.
	Ref<Material> create(GFX::GfxContext &ctx, const MaterialData &data);
	/// The low level version: `shader_path` is what to compile (resolve_shader_path), `info` the render state.
	/// Null when the shader doesn't compile.
	Ref<Material> create(GFX::GfxContext &ctx, const std::string &shader_path, MaterialCreateInfo info);

	/// For fallbacks: once `material->get_data().shader` compiles, rebuilds `material` in place with it. Does nothing
	/// when that shader isn't a failed one.
	void rebuild_when_fixed(const Ref<Material> &material);
	/// Compiles failed shaders again when the file, a .slang next to it or any engine shader changed since they
	/// failed. Application calls it with the hot reload poll. Main thread, outside frames (it waits for the GPU).
	void retry_failed(GFX::GfxContext &ctx);
	/// Goes up every time a failed shader compiles, so caches holding null materials know to ask again.
	[[nodiscard]] Uint64 get_revision() const { return m_revision; }

	/// The render state a domain draws with, for `data`'s blend mode and double sided.
	[[nodiscard]] static MaterialCreateInfo create_info_for(Shader::ShaderDomain domain, const MaterialData &data);
	/// What BloomSystem draws `material` with: its surface() through Aquila/LitEmissive.slang, only the emissive
	/// (times the exposure) into an RGBA16F target, depth tested (less or equal) against the scene depth without
	/// writing it. Takes MeshPushConstants and the material's set 1. Compiled the first time it's asked for, so the
	/// first frame stalls. Null for anything but Lit shaders or when it doesn't compile (logged once per edit).
	[[nodiscard]] GFX::GfxPipeline *get_emissive_pipeline(GFX::GfxContext &ctx, const Material &material);

  private:
	struct Instance {
		WeakRef<Material> material;
		MaterialCreateInfo info;
		bool masked = false;
	};

	struct Entry {
		Ref<Shader::ShaderProgram> program;
		std::vector<std::pair<MaterialCreateInfo, Ref<GFX::GfxPipeline>>> pipelines;
		Ref<Shader::ShaderProgram> depth_program;
		Ref<GFX::GfxPipeline> depth_pipeline;
		Ref<Shader::ShaderProgram> emissive_program;
		std::array<Ref<GFX::GfxPipeline>, 2> emissive_pipelines;
		bool emissive_failed = false;
		std::vector<Instance> instances;
		Uint64 watch_id = 0;
	};

	struct Failure {
		std::filesystem::file_time_type sources_time;
		std::vector<WeakRef<Material>> waiting;
	};

	void remember_failure(const std::string &shader_path);
	void adopt(GFX::GfxContext &ctx, Entry &entry, const std::string &shader_path, const Ref<Material> &material);

	Ref<GFX::GfxPipeline> get_pipeline(GFX::GfxContext &ctx, Entry &entry, const MaterialCreateInfo &info);
	/// Compiles the Depth variant on first use. Null when the shader has none or it doesn't compile.
	Ref<GFX::GfxPipeline> get_depth_pipeline(GFX::GfxContext &ctx, Entry &entry, const std::string &shader_path);
	Ref<GFX::GfxPipeline> build_depth_pipeline(GFX::GfxContext &ctx, Entry &entry);
	Ref<GFX::GfxPipeline> build_emissive_pipeline(GFX::GfxContext &ctx, Entry &entry, bool double_sided);

	Entry *get_entry(GFX::GfxContext &ctx, const std::string &shader_path);
	Ref<Material> make(GFX::GfxContext &ctx, Entry &entry, const std::string &shader_path,
					   const MaterialCreateInfo &info);

	std::unordered_map<std::string, Entry> m_entries;
	std::unordered_map<std::string, Failure> m_failures;
	Uint64 m_revision = 0;
	GFX::GfxDescriptorSetLayout *m_scene_layout = nullptr;

	Ref<GFX::GfxPipeline> build_pipeline(GFX::GfxContext &ctx, Shader::ShaderProgram &program,
										 const MaterialCreateInfo &info);

	void rebuild_entry(GFX::GfxContext &ctx, Entry &entry, const std::string &shader_path);
};

} // namespace Aquila::Graphics
