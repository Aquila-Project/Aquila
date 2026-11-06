#pragma once

#include "Aquila/Foundation/Signal.h"
#include "Aquila/UI/Widgets/Inspectable.h"

namespace Aquila::UI::Core {
class Image;
}

namespace Editor {

class EditorContext;

/// Hidden until the thumbnail is ready.
class ThumbnailContent final : public Aquila::UI::Core::IInspectorContent {
  public:
	ThumbnailContent(std::string path, EditorContext &context);
	~ThumbnailContent() override;

	void build_below(Aquila::UI::Core::PropertyGrid &grid) override;

  private:
	void show_texture();

	std::string m_path;
	EditorContext &m_context;
	Aquila::UI::Core::Image *m_image = nullptr;
	Signal<void(const std::string &)>::Connection m_ready_connection;
};

}
