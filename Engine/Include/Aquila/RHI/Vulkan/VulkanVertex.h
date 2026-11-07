#ifndef AQUILA_VULKAN_VERTEX_H
#define AQUILA_VULKAN_VERTEX_H

#include "GraphicsPCH.h"
#include "Aquila/Foundation/Math/MathTypes.h"

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

	static std::vector<VkVertexInputBindingDescription> get_binding_descriptions() {
		return { { .binding = 0, .stride = sizeof(Vertex), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX } };
	}

	static std::vector<VkVertexInputAttributeDescription> get_attribute_descriptions() {
		return {
			{ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos) },
			{ 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color) },
			{ 2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normals) },
			{ 3, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, tangent) },
			{ 4, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, texcoord) },
		};
	}
};

} // namespace Aquila::RHI
#endif
