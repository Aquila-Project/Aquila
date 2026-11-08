#pragma once

#include "Aquila/Scene/Components/ScriptComponent.h"
#include "Aquila/UI/Widgets/Inspectable.h"

namespace Editor {

class EditorContext;
class ScriptFieldRows;

/// The fields a script file declares, with their defaults, read only. Read when the section is built, not when the
/// file changes on disk.
class ScriptAssetContent final : public Aquila::UI::Core::IInspectorContent {
  public:
	ScriptAssetContent(std::string path, EditorContext &context);
	~ScriptAssetContent() override;

	void build_below(Aquila::UI::Core::PropertyGrid &grid) override;
	void refresh() override;

  private:
	Aquila::SceneManagement::Components::ScriptComponent m_script;
	EditorContext &m_context;
	Unique<ScriptFieldRows> m_rows;
};

[[nodiscard]] Option<Aquila::UI::Core::Inspectable> inspect_script_asset(const std::string &path,
																		 EditorContext &context);

}
