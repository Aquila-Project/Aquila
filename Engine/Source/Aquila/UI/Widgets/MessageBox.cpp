#include "Aquila/UI/Widgets/MessageBox.h"

#include "Aquila/UI/Core/LayoutLoader.h"
#include "Aquila/UI/Core/UIResources.h"

namespace Aquila::UI::Core {

namespace {

constexpr std::string_view K_COMPONENTS = "message_box";

std::string_view component_for(MessageKind kind) {
	switch (kind) {
	case MessageKind::Info:
		return "InfoBox";
	case MessageKind::Warning:
		return "WarningBox";
	case MessageKind::Error:
		return "ErrorBox";
	}
	return "InfoBox";
}

}

View *create_message_box(View &parent, MessageKind kind, std::string text) {
	LayoutLoader *loader = find_layout_loader(parent);
	if (loader == nullptr) {
		AQUILA_LOG_ERROR("MessageBox: '{}' needs a canvas with a layout loader", text);
		return nullptr;
	}
	return loader->create(engine_component_file(K_COMPONENTS), component_for(kind), parent,
						  ComponentArgs{ .attributes = { { "text", std::move(text) } } });
}

}
