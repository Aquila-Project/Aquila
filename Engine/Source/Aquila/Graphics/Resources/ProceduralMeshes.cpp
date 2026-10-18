#include "Aquila/Graphics/Resources/ProceduralMeshes.h"

#include "Aquila/Foundation/Math/Math.h"

namespace Aquila::Graphics::Resources {

namespace {

constexpr Vec3 k_white = { 1, 1, 1 };

Uint32 count(const MeshParams &params, std::string_view name) {
	return static_cast<Uint32>(params.find(name)->second);
}

F32 number(const MeshParams &params, std::string_view name) {
	return params.find(name)->second;
}

struct CubeFace {
	Vec3 normal;
	Vec3 up;
};

constexpr std::array<CubeFace, 6> k_cube_faces = { {
	{ { 0, 0, 1 }, { 0, 1, 0 } },
	{ { 0, 0, -1 }, { 0, 1, 0 } },
	{ { 1, 0, 0 }, { 0, 1, 0 } },
	{ { -1, 0, 0 }, { 0, 1, 0 } },
	{ { 0, 1, 0 }, { 0, 0, 1 } },
	{ { 0, -1, 0 }, { 0, 0, -1 } },
} };

constexpr std::array<Vec2, 4> k_quad_uvs = { Vec2(0, 0), Vec2(1, 0), Vec2(1, 1), Vec2(0, 1) };

MeshData make_cube(const MeshParams &params) {
	const F32 h = number(params, "size") * 0.5F;
	MeshData data;
	for (const CubeFace &face : k_cube_faces) {
		const Vec3 right = Math::cross(face.normal, face.up);
		const auto first = static_cast<Uint32>(data.vertices.size());
		for (const Vec2 &uv : k_quad_uvs) {
			const Vec3 pos = (face.normal + right * (uv.x * 2.F - 1.F) + face.up * (1.F - uv.y * 2.F)) * h;
			data.vertices.push_back({ pos, k_white, face.normal, uv, Vec4(right, -1) });
		}
		data.indices.insert(data.indices.end(), { first, first + 1, first + 2, first + 2, first + 3, first });
	}
	return data;
}

MeshData make_sphere(const MeshParams &params) {
	const F32 radius = number(params, "radius");
	const Uint32 segments = count(params, "segments");
	const Uint32 rings = count(params, "rings");
	const Uint32 stride = segments + 1;
	MeshData data;

	for (Uint32 stack = 0; stack <= rings; ++stack) {
		const F32 v = F32(stack) / F32(rings);
		const F32 phi = Math::PI * v;
		const bool pole = stack == 0 || stack == rings;
		for (Uint32 slice = 0; slice <= segments; ++slice) {
			const F32 u = (F32(slice) + (pole ? 0.5F : 0.F)) / F32(segments);
			const F32 theta = Math::TAU * u;
			const Vec3 normal = pole
				? Vec3(0, stack == 0 ? 1 : -1, 0)
				: Vec3(Math::sin(phi) * Math::cos(theta), Math::cos(phi), Math::sin(phi) * Math::sin(theta));
			const Vec4 tangent = { -Math::sin(theta), 0, Math::cos(theta), -1 };
			data.vertices.push_back({ normal * radius, k_white, normal, { u, v }, tangent });
		}
	}

	for (Uint32 stack = 0; stack < rings; ++stack) {
		for (Uint32 slice = 0; slice < segments; ++slice) {
			const Uint32 i0 = stack * stride + slice;
			const Uint32 i1 = i0 + 1;
			const Uint32 i2 = i0 + stride + 1;
			const Uint32 i3 = i0 + stride;
			if (stack == 0) {
				data.indices.insert(data.indices.end(), { i0, i2, i3 });
			} else if (stack + 1 == rings) {
				data.indices.insert(data.indices.end(), { i0, i1, i3 });
			} else {
				data.indices.insert(data.indices.end(), { i0, i1, i2, i0, i2, i3 });
			}
		}
	}
	return data;
}

MeshData make_cylinder(const MeshParams &params) {
	const F32 radius = number(params, "radius");
	const F32 half = number(params, "height") * 0.5F;
	const Uint32 segments = count(params, "segments");
	MeshData data;

	for (Uint32 i = 0; i <= segments; i++) {
		const F32 u = F32(i) / F32(segments);
		const F32 angle = Math::TAU * u;
		const Vec3 n = { Math::cos(angle), 0, Math::sin(angle) };
		const Vec4 t = { -Math::sin(angle), 0, Math::cos(angle), -1 };
		data.vertices.push_back({ { n.x * radius, half, n.z * radius }, k_white, n, { u, 0 }, t });
		data.vertices.push_back({ { n.x * radius, -half, n.z * radius }, k_white, n, { u, 1 }, t });
	}
	for (Uint32 i = 0; i < segments; i++) {
		const Uint32 tc = i * 2;
		const Uint32 bc = tc + 1;
		const Uint32 tn = tc + 2;
		const Uint32 bn = tn + 1;
		data.indices.insert(data.indices.end(), { tc, tn, bc, tn, bn, bc });
	}

	for (const F32 side : { 1.F, -1.F }) {
		const auto center = static_cast<Uint32>(data.vertices.size());
		const Vec3 n = { 0, side, 0 };
		const Vec4 t = { 1, 0, 0, -1 };
		data.vertices.push_back({ { 0, half * side, 0 }, k_white, n, { 0.5F, 0.5F }, t });
		for (Uint32 i = 0; i < segments; i++) {
			const F32 angle = Math::TAU * F32(i) / F32(segments);
			const F32 c = Math::cos(angle);
			const F32 s = Math::sin(angle);
			data.vertices.push_back({ { c * radius, half * side, s * radius },
									  k_white,
									  n,
									  { 0.5F + 0.5F * c, 0.5F - 0.5F * s * side },
									  t });
		}
		for (Uint32 i = 0; i < segments; i++) {
			const Uint32 curr = center + 1 + i;
			const Uint32 next = center + 1 + (i + 1) % segments;
			if (side > 0.F) {
				data.indices.insert(data.indices.end(), { center, next, curr });
			} else {
				data.indices.insert(data.indices.end(), { center, curr, next });
			}
		}
	}
	return data;
}

MeshData make_plane(const MeshParams &params) {
	const F32 width = number(params, "width");
	const F32 depth = number(params, "depth");
	const Uint32 w_segs = count(params, "width_segments");
	const Uint32 d_segs = count(params, "depth_segments");
	MeshData data;

	for (Uint32 y = 0; y <= d_segs; y++) {
		for (Uint32 x = 0; x <= w_segs; x++) {
			const F32 u = F32(x) / F32(w_segs);
			const F32 v = F32(y) / F32(d_segs);
			data.vertices.push_back({ { (u - 0.5F) * width, 0, (v - 0.5F) * depth },
									  k_white,
									  { 0, 1, 0 },
									  { u, 1.F - v },
									  { 1, 0, 0, -1 } });
		}
	}
	for (Uint32 y = 0; y < d_segs; y++) {
		for (Uint32 x = 0; x < w_segs; x++) {
			const Uint32 tl = y * (w_segs + 1) + x;
			const Uint32 tr = tl + 1;
			const Uint32 bl = (y + 1) * (w_segs + 1) + x;
			const Uint32 br = bl + 1;
			data.indices.insert(data.indices.end(), { tl, bl, tr, tr, bl, br });
		}
	}
	return data;
}

MeshParam length(std::string name, std::string label, F32 value) {
	return { .name = std::move(name),
			 .label = std::move(label),
			 .value = value,
			 .min = 0.001F,
			 .max = 1000.F,
			 .integer = false,
			 .unit = "m" };
}

MeshParam steps(std::string name, std::string label, F32 value, F32 min) {
	return { .name = std::move(name),
			 .label = std::move(label),
			 .value = value,
			 .min = min,
			 .max = 256.F,
			 .integer = true,
			 .unit = {} };
}

void build_type(const MeshGenerator &generator, Reflection::TypeInfo &type) {
	for (const MeshParam &param : generator.params) {
		Reflection::Property property;
		property.name = param.label;
		property.kind = param.integer ? Reflection::PropertyKind::Int : Reflection::PropertyKind::Float;
		property.hints = {
			.min = param.min,
			.max = param.max,
			.speed = param.integer ? 0.2F : 0.01F,
			.precision = param.integer ? 0 : 3,
			.color = false,
			.toggle = false,
			.slider = false,
			.read_only = false,
			.unit = param.unit,
			.asset_type = {},
		};
		property.get = [&param](const void *instance) -> Reflection::PropertyValue {
			const auto &params = *static_cast<const MeshParams *>(instance);
			const auto it = params.find(param.name);
			const F32 value = it != params.end() ? it->second : param.value;
			if (param.integer) {
				return static_cast<Int32>(value);
			}
			return value;
		};
		property.set = [&param](void *instance, const Reflection::PropertyValue &value) {
			auto &params = *static_cast<MeshParams *>(instance);
			const F32 number = param.integer ? static_cast<F32>(std::get<Int32>(value)) : std::get<F32>(value);
			params[param.name] = Math::clamp(number, param.min, param.max);
		};
		type.add(std::move(property));
	}
}

}

MeshGenerators &MeshGenerators::instance() {
	static MeshGenerators generators;
	return generators;
}

MeshGenerators::MeshGenerators() {
	add({ .id = "cube", .title = "Cube", .params = { length("size", "Size", 1.F) }, .generate = make_cube });
	add({ .id = "sphere",
		  .title = "Sphere",
		  .params = { length("radius", "Radius", 0.5F), steps("segments", "Segments", 32.F, 3.F),
					  steps("rings", "Rings", 16.F, 2.F) },
		  .generate = make_sphere });
	add({ .id = "cylinder",
		  .title = "Cylinder",
		  .params = { length("radius", "Radius", 0.5F), length("height", "Height", 1.F),
					  steps("segments", "Segments", 32.F, 3.F) },
		  .generate = make_cylinder });
	add({ .id = "plane",
		  .title = "Plane",
		  .params = { length("width", "Width", 1.F), length("depth", "Depth", 1.F),
					  steps("width_segments", "Width Segments", 1.F, 1.F),
					  steps("depth_segments", "Depth Segments", 1.F, 1.F) },
		  .generate = make_plane });
}

void MeshGenerators::add(MeshGenerator generator) {
	auto entry = std::make_unique<Entry>();
	entry->generator = std::move(generator);
	build_type(entry->generator, entry->type);

	for (Unique<Entry> &existing : m_entries) {
		if (existing->generator.id == entry->generator.id) {
			existing = std::move(entry);
			return;
		}
	}
	m_entries.push_back(std::move(entry));
}

const MeshGenerator *MeshGenerators::find(std::string_view id) const {
	for (const Unique<Entry> &entry : m_entries) {
		if (entry->generator.id == id) {
			return &entry->generator;
		}
	}
	return nullptr;
}

std::vector<const MeshGenerator *> MeshGenerators::all() const {
	std::vector<const MeshGenerator *> out;
	out.reserve(m_entries.size());
	for (const Unique<Entry> &entry : m_entries) {
		out.push_back(&entry->generator);
	}
	return out;
}

Option<MeshParams> MeshGenerators::resolve(std::string_view id, const MeshParams &params) const {
	const MeshGenerator *generator = find(id);
	if (generator == nullptr) {
		return std::nullopt;
	}
	MeshParams resolved;
	for (const MeshParam &param : generator->params) {
		const auto it = params.find(param.name);
		F32 value = it != params.end() && std::isfinite(it->second) ? it->second : param.value;
		value = Math::clamp(value, param.min, param.max);
		resolved[param.name] = param.integer ? std::round(value) : value;
	}
	return resolved;
}

Option<MeshData> MeshGenerators::generate(const MeshSource &source) const {
	if (source.kind != MeshSourceKind::Procedural) {
		return std::nullopt;
	}
	const MeshGenerator *generator = find(source.generator);
	Option<MeshParams> params = resolve(source.generator, source.params);
	if (generator == nullptr || !params || !generator->generate) {
		return std::nullopt;
	}
	return generator->generate(*params);
}

const Reflection::TypeInfo *MeshGenerators::get_type(std::string_view id) const {
	for (const Unique<Entry> &entry : m_entries) {
		if (entry->generator.id == id) {
			return &entry->type;
		}
	}
	return nullptr;
}

}
