#include "Aquila/Graphics/Material/MaterialData.h"
#include "Aquila/Foundation/EnginePaths.h"
#include "Aquila/Foundation/Macros.h"
#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"

namespace Aquila::Graphics {

namespace {

constexpr int k_material_version = 1;

std::string_view blend_mode_name(BlendMode mode) {
	switch (mode) {
	case BlendMode::Masked:
		return "masked";
	case BlendMode::Transparent:
		return "transparent";
	case BlendMode::Opaque:
		break;
	}
	return "opaque";
}

BlendMode blend_mode_from(std::string_view name) {
	if (name == "masked") {
		return BlendMode::Masked;
	}
	if (name == "transparent") {
		return BlendMode::Transparent;
	}
	return BlendMode::Opaque;
}

nlohmann::ordered_json value_to_json(const MaterialValue &value) {
	return std::visit(
		[](const auto &stored) -> nlohmann::ordered_json {
			using V = std::decay_t<decltype(stored)>;
			if constexpr (std::is_same_v<V, TextureReference>) {
				return { { "texture", stored.path } };
			} else if constexpr (std::is_same_v<V, Vec2>) {
				return { stored.x, stored.y };
			} else if constexpr (std::is_same_v<V, Vec3>) {
				return { stored.x, stored.y, stored.z };
			} else if constexpr (std::is_same_v<V, Vec4>) {
				return { stored.x, stored.y, stored.z, stored.w };
			} else {
				return stored;
			}
		},
		value);
}

Option<MaterialValue> value_from_json(const nlohmann::ordered_json &json) {
	if (json.is_boolean()) {
		return json.get<bool>();
	}
	if (json.is_number()) {
		return json.get<F32>();
	}
	if (json.is_object() && json.contains("texture") && json["texture"].is_string()) {
		return TextureReference{ json["texture"].get<std::string>() };
	}
	if (json.is_array() && json.size() >= 2 && json.size() <= 4 &&
		std::ranges::all_of(json, [](const auto &element) { return element.is_number(); })) {
		if (json.size() == 2) {
			return Vec2(json[0].get<F32>(), json[1].get<F32>());
		}
		if (json.size() == 3) {
			return Vec3(json[0].get<F32>(), json[1].get<F32>(), json[2].get<F32>());
		}
		return Vec4(json[0].get<F32>(), json[1].get<F32>(), json[2].get<F32>(), json[3].get<F32>());
	}
	return std::nullopt;
}

}

bool is_srgb_texture_name(std::string_view parameter_name) {
	std::string lowered(parameter_name);
	std::ranges::transform(lowered, lowered.begin(),
						   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	constexpr std::array<std::string_view, 7> k_color_words = { "color", "colour", "tint", "albedo",
																 "diffuse", "emissive", "emission" };
	return std::ranges::any_of(k_color_words,
							   [&lowered](std::string_view word) { return lowered.find(word) != std::string::npos; });
}

nlohmann::ordered_json material_to_json(const MaterialData &data) {
	nlohmann::ordered_json json;
	json["version"] = k_material_version;
	json["shader"] = portable_shader_path(data.shader);
	json["double_sided"] = data.double_sided;
	json["blend_mode"] = std::string(blend_mode_name(data.blend_mode));
	json["alpha_cutoff"] = data.alpha_cutoff;
	json["emit_light"] = data.emit_light;
	nlohmann::ordered_json values = nlohmann::ordered_json::object();
	for (const auto &[name, value] : data.values) {
		values[name] = value_to_json(value);
	}
	json["values"] = std::move(values);
	return json;
}

MaterialData material_from_json(const nlohmann::ordered_json &json) {
	MaterialData data;
	if (!json.is_object()) {
		return data;
	}
	if (json.contains("shader") && json["shader"].is_string() && !json["shader"].get<std::string>().empty()) {
		data.shader = json["shader"].get<std::string>();
	}
	data.double_sided = json.value("double_sided", false);
	data.blend_mode = blend_mode_from(json.value("blend_mode", std::string("opaque")));
	data.alpha_cutoff = json.value("alpha_cutoff", data.alpha_cutoff);
	data.emit_light = json.value("emit_light", data.emit_light);
	if (json.contains("values") && json["values"].is_object()) {
		for (const auto &[name, value] : json["values"].items()) {
			if (Option<MaterialValue> parsed = value_from_json(value)) {
				data.values[name] = std::move(*parsed);
			}
		}
	}
	return data;
}

bool load_material_file(const std::string &path, MaterialData &out) {
	const std::string text = Platform::Filesystem::VirtualFileSystem::get()->read_text_file(path);
	if (text.empty()) {
		AQUILA_LOG_WARNING("Material: can't read '{}'", path);
		return false;
	}
	const nlohmann::ordered_json json = nlohmann::ordered_json::parse(text, nullptr, false);
	if (json.is_discarded() || !json.is_object()) {
		AQUILA_LOG_WARNING("Material: '{}' is not valid JSON", path);
		return false;
	}
	out = material_from_json(json);
	return true;
}

bool save_material_file(const std::string &path, const MaterialData &data) {
	if (!Platform::Filesystem::VirtualFileSystem::get()->write_text_file(path, material_to_json(data).dump(4))) {
		AQUILA_LOG_WARNING("Material: can't write '{}'", path);
		return false;
	}
	return true;
}

std::string resolve_shader_path(const std::string &shader) {
	if (shader.empty()) {
		return Foundation::EnginePaths::shaders_dir() + std::string(STANDARD_SHADER);
	}
	if (shader.front() == '/' || shader.find("://") != std::string::npos ||
		std::filesystem::path(shader).is_absolute()) {
		return shader;
	}
	return Foundation::EnginePaths::shaders_dir() + shader;
}

std::string portable_shader_path(const std::string &path) {
	const std::string &engine_dir = Foundation::EnginePaths::shaders_dir();
	if (path.starts_with(engine_dir)) {
		return path.substr(engine_dir.size());
	}
	return path;
}

}
