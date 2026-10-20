#ifndef AQUILA_GRAPHICS_MATERIAL_DATA_H
#define AQUILA_GRAPHICS_MATERIAL_DATA_H

#include "Aquila/Foundation/PrimitiveTypes.h"
#include "json.hpp"

namespace Aquila::Graphics {

/// The built-in lit material, relative to the engine shaders folder like every engine shader.
inline constexpr std::string_view STANDARD_SHADER = "Basic.slang";
inline constexpr std::string_view UNLIT_SHADER = "Unlit.slang";
inline constexpr std::string_view MATERIAL_EXTENSION = ".aqmat";

/// A texture parameter's value: a VFS path ("/project/Content/brick.png"). Empty means the shader's default.
struct TextureReference {
	std::string path;

	bool operator==(const TextureReference &) const = default;
};

using MaterialValue = std::variant<F32, Int32, bool, Vec2, Vec3, Vec4, TextureReference>;

/// Opaque draws with the scene. Masked throws away pixels whose alpha is under `alpha_cutoff` (leaves, fences),
/// shadows included. Transparent blends over what's behind it after the sky, sorted back to front, and casts no
/// shadows.
enum class BlendMode : Uint8 { Opaque, Masked, Transparent };

/// What a material is made of: a shader and the values of the parameters it declares. Saved in .aqmat files, and
/// in scenes for an entity's own material.
///
/// `shader` is relative to the engine shaders folder for engine shaders ("Basic.slang"), a VFS path for project
/// ones ("/project/Content/Shaders/Water.slang"). `values` only holds what differs from the shader's defaults, by
/// parameter name, and keeps values of parameters the shader doesn't have (anymore), so switching shaders back and
/// forth loses nothing.
///
/// With `emit_light` a glowing mesh also lights what's around it (Material::get_average_emission, SceneFrameData).
struct MaterialData {
	std::string shader = std::string(STANDARD_SHADER);
	std::map<std::string, MaterialValue> values;
	bool double_sided = false;
	BlendMode blend_mode = BlendMode::Opaque;
	F32 alpha_cutoff = 0.5F;
	bool emit_light = true;

	bool operator==(const MaterialData &) const = default;
};

/// Texture parameters named like colors (baseColorMap, albedo, diffuse, emissiveMap...) hold sRGB images, the rest
/// hold data (normals, roughness, masks) and are read as is.
[[nodiscard]] bool is_srgb_texture_name(std::string_view parameter_name);

[[nodiscard]] nlohmann::ordered_json material_to_json(const MaterialData &data);
/// Missing keys keep their defaults. Numbers come back as F32, Material::apply converts them to the parameter type.
[[nodiscard]] MaterialData material_from_json(const nlohmann::ordered_json &json);

/// `path` is a VFS path. Both log what went wrong.
bool load_material_file(const std::string &path, MaterialData &out);
bool save_material_file(const std::string &path, const MaterialData &data);

/// What to compile for `shader`: engine shaders become full paths in the engine shaders folder, anything else
/// comes back as is.
[[nodiscard]] std::string resolve_shader_path(const std::string &shader);
/// The other way, for saving: full engine shader paths become relative again.
[[nodiscard]] std::string portable_shader_path(const std::string &path);

} // namespace Aquila::Graphics

#endif
