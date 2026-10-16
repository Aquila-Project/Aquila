#pragma once

#include "Aquila/Scene/Entity.h"

namespace Editor {

inline constexpr std::string_view k_empty_entity_icon = "circle-dashed";

[[nodiscard]] std::string_view component_icon(std::string_view component);
[[nodiscard]] std::string_view entity_icon(Aquila::SceneManagement::Entity entity);

}
