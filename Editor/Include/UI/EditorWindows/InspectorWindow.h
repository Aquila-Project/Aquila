#pragma once

#include "Core/EditorWindow.h"

namespace Aquila::UI::Core {
class InspectorView;
}

namespace Editor {

class EntityInspector;

/// A selected file stays shown until an entity is selected.
class InspectorWindow final : public EditorWindow {
  public:
	explicit InspectorWindow(EditorContext &context);
	~InspectorWindow() override;

	void build(Aquila::UI::Core::View &content) override;
	void update(F32 delta_time) override;
	void open_add_search(Vec2 canvas_pos);

  private:
	void show_entity(Aquila::SceneManagement::Entity entity);
	void show_asset(const std::string &path);

	Unique<EntityInspector> m_entities;
	Unique<Aquila::UI::Core::InspectorView> m_view;
	std::string m_asset;
};

}
