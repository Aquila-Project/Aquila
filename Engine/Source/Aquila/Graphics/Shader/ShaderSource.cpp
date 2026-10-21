#include "Aquila/Graphics/Shader/ShaderSource.h"
#include "Aquila/Foundation/Text/StringUtils.h"
#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"

namespace Aquila::Graphics::Shader {

namespace {

bool read_native(const std::string &path, std::string &out) {
	std::ifstream file(path);
	if (!file.is_open()) {
		return false;
	}
	out.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
	return true;
}

std::string line_marker(const std::string &file) {
	std::string escaped;
	for (const char c : file) {
		if (c == '\\' || c == '"') {
			escaped += '\\';
		}
		escaped += c;
	}
	return "#line 1 \"" + escaped + "\"\n";
}

const char *prelude(ShaderDomain domain) {
	switch (domain) {
	case ShaderDomain::Lit:
		return "import Utils;\nimport Utility.FrameData;\nimport Utility.LightData;\nimport Utility.SurfaceData;\n"
			   "import Utility.ClusterData;\nimport Utility.EnvironmentData;\nimport Utility.ShadowData;\n"
			   "import Aquila.Surface;\nimport Aquila.Lit;\nusing namespace Aquila::Shading;\n";
	case ShaderDomain::Unlit:
		return "import Utils;\nimport Utility.FrameData;\nimport Aquila.Surface;\nimport Aquila.Unlit;\n"
			   "using namespace Aquila::Shading;\n";
	case ShaderDomain::Fullscreen:
		return "import Utils;\nimport Utility.FrameData;\nimport Aquila.Fullscreen;\n"
			   "using namespace Aquila::Shading;\n";
	case ShaderDomain::Custom:
		break;
	}
	return "";
}

const char *entry_module_for(ShaderDomain domain, ShaderVariant variant) {
	if (variant == ShaderVariant::Emissive) {
		return domain == ShaderDomain::Lit ? "Aquila/LitEmissive" : nullptr;
	}
	const bool depth = variant == ShaderVariant::Depth;
	switch (domain) {
	case ShaderDomain::Lit:
		return depth ? "Aquila/LitDepth" : "Aquila/LitMain";
	case ShaderDomain::Unlit:
		return depth ? "Aquila/UnlitDepth" : "Aquila/UnlitMain";
	case ShaderDomain::Fullscreen:
		return depth ? nullptr : "Aquila/FullscreenMain";
	case ShaderDomain::Custom:
		break;
	}
	return nullptr;
}

const char *entry_export(ShaderDomain domain) {
	switch (domain) {
	case ShaderDomain::Lit:
		return "export void aquilaSurface(inout Surface s, SurfaceInput input) {\n\tsurface(s, input);\n}\n";
	case ShaderDomain::Unlit:
		return "export float4 aquilaUnlit(SurfaceInput input) {\n\treturn unlit(input);\n}\n";
	case ShaderDomain::Fullscreen:
		return "export float4 aquilaFullscreen(FullscreenInput input) {\n\treturn fullscreen(input);\n}\n";
	case ShaderDomain::Custom:
		break;
	}
	return "";
}
}

ShaderDomain detect_shader_domain(std::string_view code) {
	while (!code.empty()) {
		const Usize end = code.find('\n');
		std::string_view line = Foundation::trim(code.substr(0, end));
		code = end == std::string_view::npos ? std::string_view{} : code.substr(end + 1);
		if (!line.starts_with("import")) {
			continue;
		}
		line.remove_prefix(6);
		if (line.empty() || std::isspace(static_cast<unsigned char>(line.front())) == 0) {
			continue;
		}
		std::string module;
		for (const char c : line) {
			if (c == ';') {
				break;
			}
			if (std::isspace(static_cast<unsigned char>(c)) == 0) {
				module += c;
			}
		}
		if (module == "Aquila.Lit") {
			return ShaderDomain::Lit;
		}
		if (module == "Aquila.Unlit") {
			return ShaderDomain::Unlit;
		}
		if (module == "Aquila.Fullscreen") {
			return ShaderDomain::Fullscreen;
		}
	}
	return ShaderDomain::Custom;
}

std::string_view domain_name(ShaderDomain domain) {
	switch (domain) {
	case ShaderDomain::Lit:
		return "Lit";
	case ShaderDomain::Unlit:
		return "Unlit";
	case ShaderDomain::Fullscreen:
		return "Fullscreen";
	case ShaderDomain::Custom:
		break;
	}
	return "Custom";
}

std::string_view variant_name(ShaderVariant variant) {
	switch (variant) {
	case ShaderVariant::Main:
		break;
	case ShaderVariant::Depth:
		return "depth";
	case ShaderVariant::Emissive:
		return "emissive";
	}
	return {};
}

std::vector<ShaderVariant> domain_variants(ShaderDomain domain) {
	switch (domain) {
	case ShaderDomain::Lit:
		return { ShaderVariant::Main, ShaderVariant::Depth, ShaderVariant::Emissive };
	case ShaderDomain::Unlit:
		return { ShaderVariant::Main, ShaderVariant::Depth };
	case ShaderDomain::Fullscreen:
	case ShaderDomain::Custom:
		break;
	}
	return { ShaderVariant::Main };
}

std::span<const std::string_view> entry_modules() {
	static constexpr std::array<std::string_view, 6> K_MODULES = {
		"Aquila/LitMain",	"Aquila/LitDepth",	 "Aquila/LitEmissive",
		"Aquila/UnlitMain", "Aquila/UnlitDepth", "Aquila/FullscreenMain",
	};
	return K_MODULES;
}

bool load_shader_source(const std::string &path, ShaderSource &out, std::string &error, ShaderVariant variant) {
	auto *vfs = Platform::Filesystem::VirtualFileSystem::get();
	const std::string native = vfs->to_native_path(path).value_or(path);

	std::string user_code;
	std::string directory;
	if (read_native(native, user_code)) {
		directory = std::filesystem::path(native).parent_path().string();
	} else {
		user_code = vfs->read_text_file(path);
		if (user_code.empty()) {
			error = "Can't read shader '" + path + "'";
			return false;
		}
	}

	if (!compose_shader_source(path, user_code, directory, variant, out, error)) {
		return false;
	}
	out.watch_path = directory.empty() ? std::string() : native;
	return true;
}

bool compose_shader_source(const std::string &name, const std::string &code, const std::string &directory,
						   ShaderVariant variant, ShaderSource &out, std::string &error) {
	out.name = name;
	out.domain = detect_shader_domain(code);
	out.search_paths.clear();
	if (!directory.empty()) {
		out.search_paths.push_back(directory);
	}
	out.watch_path.clear();

	const bool has_depth_variant = out.domain == ShaderDomain::Lit || out.domain == ShaderDomain::Unlit;
	if (variant == ShaderVariant::Depth && !has_depth_variant) {
		error = "'" + name + "' is a " + std::string(domain_name(out.domain)) + " shader, it has no depth variant";
		return false;
	}
	if (variant == ShaderVariant::Emissive && out.domain != ShaderDomain::Lit) {
		error = "'" + name + "' is a " + std::string(domain_name(out.domain)) + " shader, it has no emissive variant";
		return false;
	}

	out.code = std::string(prelude(out.domain)) + line_marker(name) + code + "\n";
	if (out.domain != ShaderDomain::Custom) {
		out.code += line_marker("Aquila " + std::string(domain_name(out.domain)) + " entry point");
		out.code += entry_export(out.domain);
	}
	const char *module = entry_module_for(out.domain, variant);
	out.entry_module = module != nullptr ? module : "";
	return true;
}
}
