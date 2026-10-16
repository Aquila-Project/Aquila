#pragma once

#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Aquila::GFX {
class GfxTexture;
}

namespace Aquila::UI::Core {

class IResourceResolver {
  public:
	virtual ~IResourceResolver() = default;

	[[nodiscard]] virtual GFX::GfxTexture *resolve_texture(const std::string &path) const = 0;
	[[nodiscard]] virtual Delegate<void()> resolve_command(const std::string &name) const = 0;
};

} // namespace Aquila::UI::Core
