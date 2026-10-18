#pragma once
#include "Aquila/Foundation/Defines.h"
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/GFX/GfxContext.h"
#include "Aquila/GFX/GfxMesh.h"
#include "Aquila/Graphics/Resources/Mesh.h"
#include "Aquila/Rendering/Systems/Base/IRenderingSystem.h"
#include "Aquila/Rendering/FrameContext.h"

namespace Aquila::Rendering {

// RenderingSystemBase
//
// Convenient base for concrete rendering systems. Adds:
//   - m_Ctx pointer set during OnInit so subclasses don't hold it separately.
//   - GetOrUploadMesh() — uploads a CPU Mesh to the GPU on first call and caches the
//     result by pointer, avoiding redundant uploads across frames and systems.
//
// Subclasses must implement AddPasses(). OnInit / OnShutdown are open for extension
// (call the base version when you do).
class RenderingSystemBase : public IRenderingSystem {
  public:
	RenderingSystemBase() = default;
	~RenderingSystemBase() override = default;

	AQUILA_NONCOPYABLE(RenderingSystemBase);
	AQUILA_NONMOVEABLE(RenderingSystemBase);

	void on_init(GFX::GfxContext &ctx) override { m_ctx = &ctx; }

	void on_shutdown() override { m_mesh_cache.clear(); }

  protected:
	/// Uploads again when the mesh's revision changed. Copies of meshes that died are dropped on the next upload,
	/// their buffers are freed once the GPU is done with them.
	Ref<GFX::GfxMesh> get_or_upload_mesh(const Ref<Graphics::Resources::Mesh> &mesh) {
		auto it = m_mesh_cache.find(mesh.get());
		if (it != m_mesh_cache.end() && it->second.mesh.lock() == mesh && it->second.revision == mesh->get_revision()) {
			return it->second.gpu;
		}
		std::erase_if(m_mesh_cache, [](const auto &entry) { return entry.second.mesh.expired(); });
		auto gpu = GFX::GfxMesh::create(*m_ctx, mesh->get_vertices(), mesh->get_indices());
		m_mesh_cache[mesh.get()] = { .mesh = mesh, .revision = mesh->get_revision(), .gpu = gpu };
		return gpu;
	}

	GFX::GfxContext *m_ctx = nullptr;

  private:
	struct UploadedMesh {
		std::weak_ptr<Graphics::Resources::Mesh> mesh;
		Uint64 revision = 0;
		Ref<GFX::GfxMesh> gpu;
	};

	std::unordered_map<const Graphics::Resources::Mesh *, UploadedMesh> m_mesh_cache;
};

} // namespace Aquila::Rendering
