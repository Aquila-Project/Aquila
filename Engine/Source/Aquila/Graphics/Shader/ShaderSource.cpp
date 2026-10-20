#include "Aquila/Graphics/Shader/ShaderSource.h"
#include "Aquila/Foundation/EnginePaths.h"
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

std::string_view trim(std::string_view text) {
	while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
		text.remove_prefix(1);
	}
	while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
		text.remove_suffix(1);
	}
	return text;
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

const char *template_file(ShaderDomain domain, ShaderVariant variant) {
	if (variant == ShaderVariant::Emissive) {
		return domain == ShaderDomain::Lit ? "Aquila/LitEmissive.slang" : nullptr;
	}
	const bool depth = variant == ShaderVariant::Depth;
	switch (domain) {
	case ShaderDomain::Lit:
		return depth ? "Aquila/LitDepth.slang" : "Aquila/LitMain.slang";
	case ShaderDomain::Unlit:
		return depth ? "Aquila/UnlitDepth.slang" : "Aquila/UnlitMain.slang";
	case ShaderDomain::Fullscreen:
		return depth ? nullptr : "Aquila/FullscreenMain.slang";
	case ShaderDomain::Custom:
		break;
	}
	return nullptr;
}

}

ShaderDomain detect_shader_domain(std::string_view code) {
	while (!code.empty()) {
		const Usize end = code.find('\n');
		std::string_view line = trim(code.substr(0, end));
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

bool load_shader_source(const std::string &path, ShaderSource &out, std::string &error, ShaderVariant variant) {
	auto *vfs = Platform::Filesystem::VirtualFileSystem::get();
	const std::string native = vfs->to_native_path(path).value_or(path);

	std::string user_code;
	if (read_native(native, user_code)) {
		out.watch_path = native;
		out.search_paths = { std::filesystem::path(native).parent_path().string() };
	} else {
		user_code = vfs->read_text_file(path);
		if (user_code.empty()) {
			error = "Can't read shader '" + path + "'";
			return false;
		}
		out.watch_path.clear();
		out.search_paths.clear();
	}

	out.name = path;
	out.domain = detect_shader_domain(user_code);
	out.code = std::string(prelude(out.domain)) + line_marker(path) + user_code;

	const bool has_depth_variant = out.domain == ShaderDomain::Lit || out.domain == ShaderDomain::Unlit;
	if (variant == ShaderVariant::Depth && !has_depth_variant) {
		error = "'" + path + "' is a " + std::string(domain_name(out.domain)) + " shader, it has no depth variant";
		return false;
	}
	if (variant == ShaderVariant::Emissive && out.domain != ShaderDomain::Lit) {
		error = "'" + path + "' is a " + std::string(domain_name(out.domain)) + " shader, it has no emissive variant";
		return false;
	}

	if (const char *main_file = template_file(out.domain, variant)) {
		const std::string template_path = Foundation::EnginePaths::shaders_dir() + main_file;
		std::string template_code;
		if (!read_native(template_path, template_code)) {
			error = "Can't read the engine's " + std::string(domain_name(out.domain)) + " template '" +
				template_path + "'";
			return false;
		}
		out.code += "\n" + line_marker(main_file) + template_code;
	}
	return true;
}

}
