#include "UI/Inspector/ThumbnailContent.h"

#include "Aquila/UI/Widgets/Image.h"
#include "Aquila/UI/Widgets/PropertyGrid.h"
#include "Core/AssetThumbnails.h"
#include "Core/EditorContext.h"

namespace Editor {

ThumbnailContent::ThumbnailContent(std::string path, EditorContext &context)
	: m_path(std::move(path)), m_context(context) {}

ThumbnailContent::~ThumbnailContent() {
	if (m_ready_connection.is_valid()) {
		m_context.thumbnails().on_ready.disconnect(m_ready_connection);
	}
}

void ThumbnailContent::build_below(Aquila::UI::Core::PropertyGrid &grid) {
	m_image = grid.add_child<Aquila::UI::Core::Image>();
	m_image->add_class("asset-inspector-preview");
	show_texture();
	m_ready_connection = m_context.thumbnails().on_ready.connect([this](const std::string &path) {
		if (path == m_path) {
			show_texture();
		}
	});
}

void ThumbnailContent::show_texture() {
	Aquila::GFX::GfxTexture *texture = m_context.thumbnails().get(m_path);
	m_image->set_texture(texture);
	m_image->set_hidden(texture == nullptr);
}

}
