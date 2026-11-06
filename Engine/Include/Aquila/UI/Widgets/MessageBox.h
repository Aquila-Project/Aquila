#ifndef AQUILA_UI_WIDGETS_MESSAGE_BOX_H
#define AQUILA_UI_WIDGETS_MESSAGE_BOX_H

#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Aquila::UI::Core {

class View;

enum class MessageKind : Uint8 { Info, Warning, Error };

/// Builds an InfoBox, WarningBox or ErrorBox from Components/message_box.aqlayout as the last child of `parent`.
/// Markup uses those components directly.
/// @return null outside a canvas with a layout loader
View *create_message_box(View &parent, MessageKind kind, std::string text);

}

#endif
