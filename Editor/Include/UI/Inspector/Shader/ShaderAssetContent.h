#pragma once

#include "Aquila/UI/Widgets/Inspectable.h"

namespace Aquila::Graphics {
class Material;
}

namespace Editor {

class EditorContext;
class MaterialParameterRows;

/// Kind, an Open in Script Editor button and the parameters the shader declares with their defaults, read only.
/// Compiles the shader into a scratch material when the section is built, not when the file changes on disk.
class ShaderAssetContent final : public Aquila::UI::Core::IInspectorContent {
  public:
	ShaderAssetContent(std::string path, EditorContext &context);
	~ShaderAssetContent() override;

	void build_below(Aquila::UI::Core::PropertyGrid &grid) override;
	void refresh() override;

  private:
	std::string m_path;
	EditorContext &m_context;
	Ref<Aquila::Graphics::Material> m_material;
	Unique<MaterialParameterRows> m_rows;
};

[[nodiscard]] Option<Aquila::UI::Core::Inspectable> inspect_shader_asset(const std::string &path,
																		 EditorContext &context);

}
