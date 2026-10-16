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

	Uint32 push_constant_size = 256;
};

/// Builds materials from Slang shaders. Every material made from the same path shares one shader
/// program, and editing the file rebuilds all of them.
///
/// @code
/// Ref<Material> mat = MaterialFactory::get()->create(ctx, shader_path, {});
/// mat->set_albedo(Vec3(0.8F, 0.2F, 0.2F)).set_roughness(0.4F);
/// @endcode
///
/// @note A hot reload rebuilds every material of a path with the MaterialCreateInfo
/// of the first create() for that path.
class MaterialFactory : public Foundation::Singleton<MaterialFactory> {
  public:
	~MaterialFactory();

	/// The scene descriptor layout every material pipeline gets. RenderPipeline sets it,
	/// create() asserts without it.
	void set_scene_layout(GFX::GfxDescriptorSetLayout &layout) { m_scene_layout = &layout; }

	/// Null when the shader doesn't compile.
	Ref<Material> create(GFX::GfxContext &ctx, const std::string &shader_path, MaterialCreateInfo info);

  private:
	struct Entry {
		Ref<Shader::ShaderProgram> program;
		MaterialCreateInfo info;
		std::vector<WeakRef<Material>> instances;
		Uint64 watch_id = 0;
	};

	std::unordered_map<std::string, Entry> m_entries;
	GFX::GfxDescriptorSetLayout *m_scene_layout = nullptr;

	Ref<GFX::GfxPipeline> build_pipeline(GFX::GfxContext &ctx, Shader::ShaderProgram &program,
											   const MaterialCreateInfo &info);

	void rebuild_entry(GFX::GfxContext &ctx, Entry &entry, const std::string &shader_path);
};

} // namespace Aquila::Graphics
