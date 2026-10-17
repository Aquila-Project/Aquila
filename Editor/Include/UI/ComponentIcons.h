#pragma once

#include "Aquila/Scene/Entity.h"

namespace Editor {

inline constexpr std::string_view k_empty_entity_icon = "circle-dashed";

/// The one table of component icons, shared by the hierarchy, the inspector and Add Component.
[[nodiscard]] std::string_view component_icon(std::string_view component);
/// Picked from the entity's most telling component.
[[nodiscard]] std::string_view entity_icon(Aquila::SceneManagement::Entity entity);

}
