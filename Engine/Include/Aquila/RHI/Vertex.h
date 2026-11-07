#ifndef AQUILA_RHI_VERTEX_H
#define AQUILA_RHI_VERTEX_H

#include "Aquila/Foundation/Math/MathTypes.h"
#include "Aquila/RHI/Backend/RHITypes.h"

namespace Aquila::RHI {

struct Vertex {
	Vec3 pos{};
	Vec3 color{};
	Vec3 normals{};
	Vec2 texcoord{};
	Vec4 tangent{};

	bool operator==(const Vertex &other) const {
		return pos == other.pos && color == other.color && normals == other.normals && texcoord == other.texcoord &&
			tangent == other.tangent;
	}

	/// What a pipeline reads when its GraphicsPipelineDesc has no custom_vertex_layout.
	static VertexBindingDesc layout() {
		const auto attribute = [](Uint32 location, TextureFormat format, Usize offset) {
			return VertexAttributeDesc{ .location = location,
										.binding = 0,
										.format = format,
										.offset = static_cast<Uint32>(offset) };
		};
		return {
			.stride = static_cast<Uint32>(sizeof(Vertex)),
			.attributes = {
				attribute(0, TextureFormat::RGB32F, offsetof(Vertex, pos)),
				attribute(1, TextureFormat::RGB32F, offsetof(Vertex, color)),
				attribute(2, TextureFormat::RGB32F, offsetof(Vertex, normals)),
				attribute(3, TextureFormat::RGBA32F, offsetof(Vertex, tangent)),
				attribute(4, TextureFormat::RG32F, offsetof(Vertex, texcoord)),
			},
		};
	}
};

}

#endif
