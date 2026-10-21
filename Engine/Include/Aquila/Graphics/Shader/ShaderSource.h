#ifndef AQUILA_GRAPHICS_SHADER_SOURCE_H
#define AQUILA_GRAPHICS_SHADER_SOURCE_H

#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Aquila::Graphics::Shader {

/// What a material shader is, picked by the engine module it imports:
/// - `import Aquila.Lit;` the shader writes `void surface(inout Surface s, SurfaceInput input)`, the engine adds the
///   vertex shader, PBR lighting, shadows and fog.
/// - `import Aquila.Unlit;` the shader writes `float4 unlit(SurfaceInput input)`, drawn as is (not exposed).
/// - `import Aquila.Fullscreen;` the shader writes `float4 fullscreen(FullscreenInput input)`, run over the whole
///   screen by a Fullscreen Effect, reading `sceneColor` and `sceneDepth`.
/// - Custom: none of those, the shader has its own [shader("vertex")]/[shader("fragment")] entry points.
///
/// Material parameters are a `cbuffer` at `[[vk::binding(0, 1)]]`, textures `Sampler2D`s at bindings 1 and up of
/// set 1. Set 0 is the engine's (camera, lights, shadows).
enum class ShaderDomain : Uint8 { Lit, Unlit, Fullscreen, Custom };

/// Main is what the material draws with. Depth wraps a Lit or Unlit shader's surface()/unlit() in a depth only
/// program that throws away pixels under the alpha cutoff, for masked materials in shadow maps. Emissive wraps a Lit
/// shader's surface() in a program that outputs only Surface::emissive (times the exposure), the source of bloom.
enum class ShaderVariant : Uint8 { Main, Depth, Emissive };

/// A material shader ready to compile.
struct ShaderSource {
	ShaderDomain domain = ShaderDomain::Custom;
	/// What compile errors call it, the path the material points at.
	std::string name;
	std::string code;
	/// Where imports are looked up besides the engine shaders folder: the shader's own folder.
	std::vector<std::string> search_paths;
	/// The file on disk hot reload watches. Empty when there's none.
	std::string watch_path;
	/// Empty for Custom shaders.
	std::string entry_module;
};

[[nodiscard]] ShaderDomain detect_shader_domain(std::string_view code);
[[nodiscard]] std::string_view domain_name(ShaderDomain domain);
/// "", "depth" or "emissive".
[[nodiscard]] std::string_view variant_name(ShaderVariant variant);
/// Main first.
[[nodiscard]] std::vector<ShaderVariant> domain_variants(ShaderDomain domain);
[[nodiscard]] std::span<const std::string_view> entry_modules();

/// `path` is an engine shader path or a VFS path. Depth fails for Fullscreen and Custom, Emissive for
/// anything but Lit.
bool load_shader_source(const std::string &path, ShaderSource &out, std::string &error,
						ShaderVariant variant = ShaderVariant::Main);

/// Same as load_shader_source() for code in memory. `directory` is where its imports are looked up, empty for
/// none. Reads no files, any thread.
bool compose_shader_source(const std::string &name, const std::string &code, const std::string &directory,
						   ShaderVariant variant, ShaderSource &out, std::string &error);
}

#endif
