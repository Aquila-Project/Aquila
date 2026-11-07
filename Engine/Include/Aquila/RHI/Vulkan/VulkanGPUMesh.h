#ifndef AQUILA_VULKAN_GPU_MESH_H
#define AQUILA_VULKAN_GPU_MESH_H

#include "GraphicsPCH.h"
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/RHI/Backend/IRHIGPUMesh.h"
#include "Aquila/RHI/Vulkan/VulkanTypes.h"
#include "Aquila/RHI/Vertex.h"

namespace Aquila::RHI {
class VulkanDevice;
class VulkanCommandList;

class VulkanGPUMesh final : public IRHIGPUMesh {
  public:
	VulkanGPUMesh(VulkanDevice &device, const GPUMeshDesc &desc);
	~VulkanGPUMesh() override;
	AQUILA_NONCOPYABLE(VulkanGPUMesh);
	AQUILA_NONMOVEABLE(VulkanGPUMesh);

	// IRHIGPUMesh
	void bind(IRHICommandList &cmd) const override;
	void draw(IRHICommandList &cmd) const override;

	[[nodiscard]] bool is_valid() const override { return m_vertex_allocation.is_valid(); }
	[[nodiscard]] Uint32 get_vertex_count() const override { return m_vertex_count; }
	[[nodiscard]] Uint32 get_index_count() const override { return m_index_count; }
	[[nodiscard]] bool has_index_buffer() const override { return m_has_index_buffer; }

  private:
	void upload_vertex_buffer(const std::vector<Vertex> &vertices);
	void upload_index_buffer(const std::vector<Uint32> &indices);

	VulkanDevice &m_device;
	std::string m_debug_name;
	BufferAllocation m_vertex_allocation{};
	BufferAllocation m_index_allocation{};
	std::vector<GPUMeshPrimitive> m_primitives;
	Uint32 m_vertex_count = 0;
	Uint32 m_index_count = 0;
	bool m_has_index_buffer = false;
};

} // namespace Aquila::RHI
#endif
