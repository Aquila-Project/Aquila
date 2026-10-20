#pragma once
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/GFX/GfxTexture.h"
#include "Aquila/Graphics/Texture/TextureLibrary.h"

namespace Aquila::Graphics {

// Typed enum for all parameter kinds a material can hold.
enum class ParameterType : Uint8 { Float, Int, Bool, Vec2, Vec3, Vec4, Texture2D, Color };

// Variant covering every concrete value a parameter can take.
// Texture2D entries are bound directly to the descriptor set;
// all other types are packed into the material UBO.
using ParameterValue = std::variant<F32, int, bool, Vec2, Vec3, Vec4, Ref<GFX::GfxTexture>>;

// One reflected or manually-registered shader parameter.
struct MaterialParameter {
	std::string name;
	ParameterType type = ParameterType::Float;
	ParameterValue value;
	ParameterValue default_value;

	// Byte offset inside the material UBO (std140 layout).
	// UINT32_MAX means this parameter is a texture, not in the UBO.
	Uint32 ubo_offset = UINT32_MAX;

	// Descriptor-set binding index for Texture2D parameters.
	// UINT32_MAX for non-texture parameters.
	Uint32 texture_binding = UINT32_MAX;

	/// Textures: decoded from sRGB (color maps) or read as is (normal, roughness...), and what's bound when the
	/// material doesn't set one. Both guessed from the name.
	bool srgb = false;
	DefaultTexture default_texture = DefaultTexture::White;
	/// Engine inputs (a fullscreen effect's sceneColor/sceneDepth): bound by the renderer, not shown or saved.
	bool hidden = false;
};

} // namespace Aquila::Graphics
