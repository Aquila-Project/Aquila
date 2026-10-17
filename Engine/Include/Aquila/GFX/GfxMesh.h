#pragma once
#include "Aquila/Foundation/Defines.h"
#include "Aquila/GFX/GfxBuffer.h"
#include "Aquila/RHI/Vertex.h"

namespace Aquila::GFX {

class GfxContext;

class GfxMesh {
  public:
	/// Uploads to GPU only buffers right away and blocks until it's done.
	static Ref<GfxMesh> create(GfxContext &ctx, std::span<const RHI::Vertex> vertices, std::span<const Uint32> indices);

	AQUILA_NONCOPYABLE(GfxMesh);
	AQUILA_NONMOVEABLE(GfxMesh);

	[[nodiscard]] GfxBuffer &get_vertex_buffer() const { return *m_vertex_buffer; }
	[[nodiscard]] GfxBuffer &get_index_buffer() const { return *m_index_buffer; }
	[[nodiscard]] Uint32 get_index_count() const { return m_index_count; }

  private:
	GfxMesh() = default;
	Ref<GfxBuffer> m_vertex_buffer;
	Ref<GfxBuffer> m_index_buffer;
	Uint32 m_index_count = 0;
};

} // namespace Aquila::GFX
