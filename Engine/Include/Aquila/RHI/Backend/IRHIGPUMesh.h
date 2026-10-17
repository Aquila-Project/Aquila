#pragma once

#include "Aquila/RHI/Vertex.h"
#include "Aquila/RHI/Backend/IRHICommandList.h"
#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Aquila::RHI {

struct GPUMeshPrimitive {
	Uint32 first_index = 0;
	Uint32 first_vertex = 0;
	Uint32 index_count = 0;
	Uint32 vertex_count = 0;
};

/// The data is uploaded at creation, the vectors only need to live during the call.
struct GPUMeshDesc {
	const std::vector<Vertex> *vertices = nullptr;
	const std::vector<Uint32> *indices = nullptr;
	const std::vector<GPUMeshPrimitive> *primitives = nullptr;
	std::string debug_name;
};

class IRHIGPUMesh {
  public:
	virtual ~IRHIGPUMesh() = default;
	AQUILA_NONCOPYABLE(IRHIGPUMesh);
	AQUILA_NONMOVEABLE(IRHIGPUMesh);

	virtual void bind(IRHICommandList &cmd) const = 0;
	/// One draw per primitive.
	virtual void draw(IRHICommandList &cmd) const = 0;

	[[nodiscard]] virtual bool is_valid() const = 0;
	[[nodiscard]] virtual Uint32 get_vertex_count() const = 0;
	[[nodiscard]] virtual Uint32 get_index_count() const = 0;
	[[nodiscard]] virtual bool has_index_buffer() const = 0;

  protected:
	IRHIGPUMesh() = default;
};

} // namespace Aquila::RHI
