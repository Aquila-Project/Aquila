#include "Aquila/Graphics/Resources/MeshSource.h"

#include "Aquila/Graphics/Resources/ProceduralMeshes.h"

#include <charconv>

namespace Aquila::Graphics::Resources {

MeshSource MeshSource::procedural(std::string generator, MeshParams params) {
	return {
		.kind = MeshSourceKind::Procedural,
		.generator = std::move(generator),
		.params = std::move(params),
		.path = {},
	};
}

MeshSource MeshSource::asset(std::string path) {
	return { .kind = MeshSourceKind::Asset, .generator = {}, .params = {}, .path = std::move(path) };
}

Option<MeshSource> MeshSource::from_legacy_path(std::string_view path) {
	constexpr std::string_view k_scheme = "procedural://";
	if (path.empty()) {
		return std::nullopt;
	}
	if (!path.starts_with(k_scheme)) {
		return asset(std::string(path));
	}
	path.remove_prefix(k_scheme.size());

	std::vector<std::string_view> parts;
	for (Usize start = 0; start <= path.size();) {
		const Usize end = std::min(path.find('/', start), path.size());
		parts.push_back(path.substr(start, end - start));
		start = end + 1;
	}

	const MeshGenerator *generator = MeshGenerators::instance().find(parts.front());
	if (generator == nullptr) {
		return std::nullopt;
	}

	MeshParams params;
	for (Usize i = 1; i < parts.size() && i - 1 < generator->params.size(); ++i) {
		F32 value = 0.F;
		const auto [ptr, error] = std::from_chars(parts[i].data(), parts[i].data() + parts[i].size(), value);
		if (error != std::errc{}) {
			return std::nullopt;
		}
		params[generator->params[i - 1].name] = value;
	}

	if (generator->id == "cube") {
		auto size = params.find("size");
		params["size"] = size != params.end() ? size->second * 2.F : 2.F;
	}
	return procedural(generator->id, std::move(params));
}

std::string MeshSource::key() const {
	switch (kind) {
	case MeshSourceKind::Procedural: {
		std::string out = "procedural:" + generator;
		for (const auto &[name, value] : params) {
			out += std::format(" {}={}", name, value);
		}
		return out;
	}
	case MeshSourceKind::Asset:
		return "asset:" + path;
	case MeshSourceKind::None:
		break;
	}
	return {};
}

std::string MeshSource::describe() const {
	switch (kind) {
	case MeshSourceKind::Procedural:
		return generator;
	case MeshSourceKind::Asset:
		return std::filesystem::path(path).filename().string();
	case MeshSourceKind::None:
		break;
	}
	return "mesh";
}

}
