#pragma once

#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Aquila::GFX {
class GfxTexture;
}

namespace Aquila::UI::Core {

struct CommandInfo {
	bool known = false;
	bool enabled = false;
	std::string title;
	std::string shortcut;
};

/// What markup can reference by name: textures for src="" and commands for on-click="".
class IResourceResolver {
  public:
	virtual ~IResourceResolver() = default;

	[[nodiscard]] virtual GFX::GfxTexture *resolve_texture(const std::string &path) const = 0;
	[[nodiscard]] virtual Delegate<void()> resolve_command(const std::string &name) const = 0;
	virtual CommandInfo query_command(std::string_view name) const = 0;
	[[nodiscard]] virtual bool has_commands() const = 0;
};

} // namespace Aquila::UI::Core
