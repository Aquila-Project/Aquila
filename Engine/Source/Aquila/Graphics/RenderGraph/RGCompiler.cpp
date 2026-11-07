#include "Aquila/Graphics/RenderGraph/RGCompiler.h"

#include "Aquila/Foundation/Macros.h"
#include "Aquila/GFX/GfxContext.h"
#include "Aquila/GFX/GfxTexture.h"
#include "Aquila/GFX/GfxBuffer.h"
#include "Aquila/GFX/GfxRenderpass.h"
#include "Aquila/RHI/Backend/RHITypes.h"

// https://themaister.net/blog/2017/08/15/render-graphs-and-vulkan-a-deep-dive/
namespace Aquila::Graphics::RG {

// bit packing magic
// its packing 2 things: resource id and its version
// 32 bit integer split into 2 :
// [8 bits for version and 24 bits for the slot]
Uint32 RGCompiler::slot_of(Uint32 id) {
	return id & 0x00FFFFFFu;
}

// shock, the function does exactly what the name says
// verifies if two texture descriptors are compatible, i.e same shape, format, etc.
bool RGCompiler::tex_desc_compatible(const RGTextureDesc &a, const RGTextureDesc &b) {
	return a.width == b.width && a.height == b.height && a.mip_levels == b.mip_levels &&
		a.array_layers == b.array_layers && a.format == b.format && a.usage == b.usage && a.samples == b.samples;
}

bool RGCompiler::buf_desc_compatible(const RGBufferDesc &a, const RGBufferDesc &b) {
	return a.size == b.size && a.usage == b.usage && a.domain == b.domain;
}

// The juicy part
// Basically this function answers: "given all those accesses, which pass must happen before which?"
// It figures out which pqsses depend on whcih, it will basically go through every pass and check who wrote texture X and who reads this texture?

// Algorithm:
//   For each pass X that writes (slot, ver):
//     - Find the pass Y that wrote (slot, ver-1)       -> edge Y to X (WAW chain)
//     - Find all passes Z that read (slot, ver-1)      -> edge Z to X (WAR)
//   For each pass X that reads (slot, ver):
//     - Find the pass Y that wrote (slot, ver)         -> edge Y to X (RAW)

RGCompiler::AdjList RGCompiler::build_dependency_graph(const std::vector<RGPassData> &passes, Uint32 tex_count,
													   Uint32 buf_count) {
	const auto nb_passes = static_cast<Uint32>(passes.size());
	AdjList adj(nb_passes);

	// (slot, version) -> writer pass index
	std::unordered_map<Uint64, Uint32> tex_writer; // who wrote? one pass per entry
	std::unordered_map<Uint64, Uint32> buf_writer;

	// (slot, version) -> [reader pass indices]
	std::unordered_map<Uint64, std::vector<Uint32>> tex_readers; // who read? multiple passes per entry
	std::unordered_map<Uint64, std::vector<Uint32>> buf_readers;

	tex_writer.reserve(tex_count * 2);
	buf_writer.reserve(buf_count * 2);

	// generate a key that has zero chance of collision
	// its the same packing as the handle, just bigger so slot and version have plenty of room and never overlap
	auto tex_key = [](Uint32 slot, Uint32 ver) -> Uint64 { return (static_cast<Uint64>(slot) << 32) | ver; };
	auto buf_key = tex_key;

	// First pass: index all writers and readers
	for (Uint32 pass_index = 0; pass_index < nb_passes; ++pass_index) {
		const RGPassData &pass = passes[pass_index];

		// example :
		// WriterPass writes texture1 v0 -> texWriter[(scene, 0)] = WriterPass => ONLY 1
		// ReaderPass reads texture1 v0 -> texReaders[(scene, 0)] = [WriterPass] => MULTIPLE

		for (const RGTextureAccess &texture_write : pass.texture_writes) {
			const Uint32 slot = slot_of(texture_write.handle.id);
			const Uint32 ver = (texture_write.handle.id >> 24) & 0xFFu;
			tex_writer[tex_key(slot, ver)] = pass_index;
		}
		for (const RGBufferAccess &buffer_write : pass.buffer_writes) {
			const Uint32 slot = slot_of(buffer_write.handle.id);
			const Uint32 ver = (buffer_write.handle.id >> 24) & 0xFFu;
			buf_writer[buf_key(slot, ver)] = pass_index;
		}
		for (const RGTextureAccess &texture_read : pass.texture_reads) {
			const Uint32 slot = slot_of(texture_read.handle.id);
			const Uint32 ver = (texture_read.handle.id >> 24) & 0xFFu;
			tex_readers[tex_key(slot, ver)].push_back(pass_index);
		}
		for (const RGBufferAccess &buffer_read : pass.buffer_reads) {
			const Uint32 slot = slot_of(buffer_read.handle.id);
			const Uint32 ver = (buffer_read.handle.id >> 24) & 0xFFu;
			buf_readers[buf_key(slot, ver)].push_back(pass_index);
		}
	}

	auto add_edge = [&](Uint32 from, Uint32 to) {
		if (from == to) {
			return;
		}
		// Deduplicate edges to keep the graph lean
		auto &deps = adj[from];
		if (std::ranges::find(deps, to) == deps.end()) {
			deps.push_back(to);
		}
	};

	// Second pass: wire edges
	for (Uint32 pass_index = 0; pass_index < nb_passes; ++pass_index) {
		const RGPassData &pass = passes[pass_index];

		// RAW: reader depends on its writer
		for (const RGTextureAccess &a : pass.texture_reads) {
			const Uint32 slot = slot_of(a.handle.id);
			const Uint32 ver = (a.handle.id >> 24) & 0xFFu;
			if (auto it = tex_writer.find(tex_key(slot, ver)); it != tex_writer.end()) {
				add_edge(it->second, pass_index);
			}
		}
		for (const RGBufferAccess &a : pass.buffer_reads) {
			const Uint32 slot = slot_of(a.handle.id);
			const Uint32 ver = (a.handle.id >> 24) & 0xFFu;
			if (auto it = buf_writer.find(buf_key(slot, ver)); it != buf_writer.end()) {
				add_edge(it->second, pass_index);
			}
		}

		// WAW chain + WAR: a write to ver V means this pass consumed ver V-1
		for (const RGTextureAccess &a : pass.texture_writes) {
			const Uint32 slot = slot_of(a.handle.id);
			const Uint32 ver = (a.handle.id >> 24) & 0xFFu;
			if (ver > 0) {
				// WAW: previous writer of (slot, ver-1) must complete before this write
				if (auto it = tex_writer.find(tex_key(slot, ver - 1)); it != tex_writer.end()) {
					add_edge(it->second, pass_index);
				}
				// WAR: all readers of (slot, ver-1) must complete before this write
				if (auto it = tex_readers.find(tex_key(slot, ver - 1)); it != tex_readers.end()) {
					for (Uint32 reader : it->second) {
						add_edge(reader, pass_index);
					}
				}
			}
		}
		for (const RGBufferAccess &a : pass.buffer_writes) {
			const Uint32 slot = slot_of(a.handle.id);
			const Uint32 ver = (a.handle.id >> 24) & 0xFFu;
			if (ver > 0) {
				if (auto it = buf_writer.find(buf_key(slot, ver - 1)); it != buf_writer.end()) {
					add_edge(it->second, pass_index);
				}
				if (auto it = buf_readers.find(buf_key(slot, ver - 1)); it != buf_readers.end()) {
					for (Uint32 reader : it->second) {
						add_edge(reader, pass_index);
					}
				}
			}
		}
	}

	return adj;
}

// Topological Sort
//
// Stability guarantee: ties broken by original pass index, ensuring the sorted
// order is deterministic across frames and platforms.
//
// Returns false if a cycle is found.  outCyclePath contains the cycle nodes.
// https://www.geeksforgeeks.org/dsa/topological-sorting-indegree-based-solution/
bool RGCompiler::topological_sort(const AdjList &adj, Uint32 pass_count, std::vector<Uint32> &out_order,
								  std::vector<Uint32> &out_cycle_path) {
	out_order.clear();
	out_order.reserve(pass_count);

	std::vector<Int32> in_degree(pass_count, 0);
	for (Uint32 i = 0; i < pass_count; ++i) {
		for (Uint32 dep : adj[i]) {
			++in_degree[dep];
		}
	}

	// Use a min-heap keyed on pass index for stability
	std::priority_queue<Uint32, std::vector<Uint32>, std::greater<>> ready;
	for (Uint32 i = 0; i < pass_count; ++i) {
		if (in_degree[i] == 0) {
			ready.push(i);
		}
	}

	while (!ready.empty()) {
		Uint32 cur = ready.top();
		ready.pop();
		out_order.push_back(cur);
		for (Uint32 succ : adj[cur]) {
			if (--in_degree[succ] == 0) {
				ready.push(succ);
			}
		}
	}

	if (out_order.size() < pass_count) {
		// gather nodes with remaining in-degree > 0 as the cycle evidence
		for (Uint32 i = 0; i < pass_count; ++i) {
			if (in_degree[i] > 0) {
				out_cycle_path.push_back(i);
			}
		}
		return false;
	}
	return true;
}

// Pass Culling
//
// This will remove any pass that is not contributing to anything visible
// It works backwards from the final outputs (basically smth like what do we actually need on screen?)
// and marks everything that feeds into those.
//
// Any pass that only writes into a texture nobody ever reads gets thrown away. therefore => GPU happy no extra work
//
// A pass is "live" if it:
//  a) writes to an imported resource,
//  OR
//  b) is reachable from a live pass through the *reverse* dependency graph.
//
// Passes that only produce transient resources consumed by dead passes are removed.

std::vector<bool> RGCompiler::cull_passes(const std::vector<RGPassData> &passes, const AdjList &adj,
										  const std::vector<Uint32> &sorted_order, const RGRegistry &registry) {
	const Uint32 n = static_cast<Uint32>(passes.size());
	std::vector<bool> alive(n, false);

	// Build reverse adjacency list
	AdjList radj(n);
	for (Uint32 i = 0; i < n; ++i) {
		for (Uint32 succ : adj[i]) {
			radj[succ].push_back(i);
		}
	}

	// Passes with an external side effect (e.g. swapchain blit) are always sinks.
	// Passes that write to an imported resource are also always sinks.
	// Passes with an unsatisfied read dependency are never sinks and can never be made alive.
	for (Uint32 pi = 0; pi < n; ++pi) {
		const RGPassData &p = passes[pi];
		if (p.has_unsatisfied_dep) {
			continue;
		}
		bool is_sink = p.has_side_effect;
		if (!is_sink) {
			for (const RGTextureAccess &a : p.texture_writes) {
				if (registry.is_imported_texture(a.handle)) {
					is_sink = true;
					break;
				}
			}
		}
		if (!is_sink) {
			for (const RGBufferAccess &a : p.buffer_writes) {
				if (registry.is_imported_buffer(a.handle)) {
					is_sink = true;
					break;
				}
			}
		}
		if (is_sink) {
			alive[pi] = true;
		}
	}

	// BFS backwards from sinks
	std::queue<Uint32> work;
	for (Uint32 i = 0; i < n; ++i) {
		if (alive[i]) {
			work.push(i);
		}
	}

	while (!work.empty()) {
		Uint32 cur = work.front();
		work.pop();
		for (Uint32 pred : radj[cur]) {
			if (!alive[pred] && !passes[pred].has_unsatisfied_dep) {
				alive[pred] = true;
				work.push(pred);
			}
		}
	}

	return alive;
}

//  Lifetime analysis
std::vector<RGCompiler::LifetimeInterval>
RGCompiler::compute_tex_lifetimes(const std::vector<RGPassData> &passes, const std::vector<Uint32> &sorted_order,
								  const std::vector<bool> &alive, Uint32 tex_count, const RGRegistry &registry) {
	std::vector<LifetimeInterval> lifetimes(tex_count);
	// Use GetTextureVersion to build a current handle for validation-safe lookups
	for (Uint32 slot = 0; slot < tex_count; ++slot) {
		const Uint32 ver = registry.get_texture_version(RGTextureHandle{ slot });

		// very confusing i know
		// but look, here is some explanation for  you
		// basic example : ver = 2 and slot = 5
		// 32 bits integer => 00000000 000000000000000000000000 => as explained before 8 bits for the version, 24 bits for the slot
		// ver << 24  = 00000010 000000000000000000000000
		// slot = 00000000 000000000000000000000101
		// OR operation => 00000010 000000000000000000000101
		//
		// this gives us the packed handle ID, that has both pieces of info packed inside version and slot :D
		const Uint32 cur_id = (ver << 24u) | (slot);

		lifetimes[slot].imported = registry.is_imported_texture(RGTextureHandle{ cur_id });
	}

	for (Int32 pos = 0; pos < static_cast<Int32>(sorted_order.size()); ++pos) {
		const Uint32 pass_index = sorted_order[pos];
		if (!alive[pass_index]) {
			continue;
		}
		const RGPassData &pass = passes[pass_index];

		// tell the texture its used at position X
		auto touch = [&](RGTextureHandle handle) {
			Uint32 slot = slot_of(handle.id);
			lifetimes[slot].first_use = std::min(lifetimes[slot].first_use, pos);
			lifetimes[slot].last_use = std::max(lifetimes[slot].last_use, pos);
		};

		for (const auto &texture_read : pass.texture_reads) {
			touch(texture_read.handle);
		}
		for (const auto &texture_write : pass.texture_writes) {
			touch(texture_write.handle);
		}
		for (const auto &color_attachment : pass.color_attachments) {
			if (color_attachment.handle.is_valid()) {
				touch(color_attachment.handle);
			}
		}
		if (pass.has_depth_attachment) {
			touch(pass.depth_attachment.handle);
		}
	}
	return lifetimes;
}

std::vector<RGCompiler::LifetimeInterval>
RGCompiler::compute_buf_lifetimes(const std::vector<RGPassData> &passes, const std::vector<Uint32> &sorted_order,
								  const std::vector<bool> &alive, Uint32 buf_count, const RGRegistry &registry) {
	// ! The usage is the same as for the textures so if you are lost, read the comments on the function above

	std::vector<LifetimeInterval> lifetimes(buf_count);
	for (Uint32 slot = 0; slot < buf_count; ++slot) {
		const Uint32 ver = registry.get_buffer_version(RGBufferHandle{ slot });
		const Uint32 cur_id = (ver << 24u) | (slot);
		lifetimes[slot].imported = registry.is_imported_buffer(RGBufferHandle{ cur_id });
	}

	for (Int32 pos = 0; pos < static_cast<Int32>(sorted_order.size()); ++pos) {
		const Uint32 pass_index = sorted_order[pos];
		if (!alive[pass_index]) {
			continue;
		}
		const RGPassData &pass = passes[pass_index];

		auto touch = [&](RGBufferHandle h) {
			Uint32 slot = slot_of(h.id);
			lifetimes[slot].first_use = std::min(lifetimes[slot].first_use, pos);
			lifetimes[slot].last_use = std::max(lifetimes[slot].last_use, pos);
		};
		for (const auto &buffer_read : pass.buffer_reads) {
			touch(buffer_read.handle);
		}
		for (const auto &buffer_write : pass.buffer_writes) {
			touch(buffer_write.handle);
		}
	}
	return lifetimes;
}

// Transient resource allocation with memory aliasing
//
// Greedy interval-coloring: resources with non-overlapping live intervals and
// compatible descriptors share the same physical allocation.
//
// Sort slots by firstUse, then for each slot try to find a free physical
// resource from the pool whose lastUsedAt < slot.firstUse and whose descriptor
// is compatible.  This achieves optimal aliasing for intervals sorted by start.

void RGCompiler::allocate_transients(const std::vector<RGPassData> &passes, RGRegistry &registry,
									 const std::vector<LifetimeInterval> &tex_lifetimes,
									 const std::vector<LifetimeInterval> &buf_lifetimes, GFX::GfxContext &ctx,
									 RGCompiledGraph &out) {
	const Uint32 tex_count = registry.texture_count();

	// build a sorted list of what needs allocating
	std::vector<std::pair<Int32, Uint32>> tex_order;
	tex_order.reserve(tex_count);
	for (Uint32 slot = 0; slot < tex_count; ++slot) {
		const LifetimeInterval &lt = tex_lifetimes[slot];
		// skip if its externally owned or never used
		if (lt.imported || lt.first_use == INT32_MAX) {
			continue; // never used or external
		}
		tex_order.emplace_back(lt.first_use, slot);
	}
	// sort by first use (super hack to make greedy recycling work optimally)
	std::ranges::stable_sort(tex_order);

	std::vector<TexPoolEntry> tex_pool;
	tex_pool.reserve(tex_order.size());

	// try to recylce whatever is recyclable, otherwise just allocate fresh
	for (auto [firstUse, slot] : tex_order) {
		const Uint32 tver = registry.get_texture_version(RGTextureHandle{ slot });
		const Uint32 t_id = (tver << 24u) | (slot);
		const RGTextureDesc &desc = registry.get_texture_desc(RGTextureHandle{ t_id });
		const Int32 last = tex_lifetimes[slot].last_use;

		// Try to find a free compatible physical texture
		TexPoolEntry *match = nullptr;
		// foreach entry in the current texture pool
		for (TexPoolEntry &entry : tex_pool) {
			// is the texture free and is it compatible?
			if (entry.last_used_at < firstUse && tex_desc_compatible(entry.desc, desc)) {
				if ((match == nullptr) || entry.last_used_at > match->last_used_at) {
					match = &entry; // Prefer most-recently-freed for better cache locality
				}
			}
		}

		// if we have a match => recycle it
		if (match != nullptr) {
			match->last_used_at = last;
			registry.resolve_texture(RGTextureHandle{ t_id }, match->tex.get());
		} else {
			// if no match => allocate a new physical texture.
			// Convert RGTextureDesc -> RHI::TextureDesc.
			RHI::TextureDesc rhi_desc{};
			rhi_desc.width = desc.width;
			rhi_desc.height = desc.height;
			rhi_desc.mip_levels = desc.mip_levels;
			rhi_desc.array_layers = desc.array_layers;
			rhi_desc.format = desc.format;
			rhi_desc.usage = desc.usage;
			rhi_desc.samples = desc.samples;
			rhi_desc.debug_name = desc.debug_name.empty() ? "RG_Transient" : std::string(desc.debug_name);

			// viewType: derive from arrayLayers and format
			if (rhi_desc.array_layers == 6) {
				rhi_desc.view_type = RHI::TextureViewType::Cube;
			} else if (rhi_desc.array_layers > 1) {
				rhi_desc.view_type = RHI::TextureViewType::Tex2DArray;
			} else {
				rhi_desc.view_type = RHI::TextureViewType::Tex2D;
			}

			Ref<GFX::GfxTexture> new_tex = ctx.create_texture(rhi_desc);
			AQUILA_ASSERT(new_tex, "Failed to allocate transient texture");

			out.transient_textures.push_back(new_tex);
			registry.resolve_texture(RGTextureHandle{ t_id }, new_tex.get());

			tex_pool.push_back(TexPoolEntry{ .tex = new_tex, .last_used_at = last, .desc = desc });
		}
	}

	// do the same for buffers
	const Uint32 buf_count = registry.buffer_count();

	std::vector<std::pair<Int32, Uint32>> buf_order;
	buf_order.reserve(buf_count);

	for (Uint32 slot = 0; slot < buf_count; ++slot) {
		const LifetimeInterval &lt = buf_lifetimes[slot];
		if (lt.imported || lt.first_use == INT32_MAX) {
			continue;
		}
		buf_order.emplace_back(lt.first_use, slot);
	}
	std::ranges::stable_sort(buf_order);

	std::vector<BufPoolEntry> buf_pool;
	buf_pool.reserve(buf_order.size());

	for (auto [firstUse, slot] : buf_order) {
		const Uint32 bver = registry.get_buffer_version(RGBufferHandle{ slot });
		const Uint32 b_id = (bver << 24u) | (slot);
		const RGBufferDesc &desc = registry.get_buffer_desc(RGBufferHandle{ b_id });
		const Int32 last = buf_lifetimes[slot].last_use;

		BufPoolEntry *match = nullptr;
		for (BufPoolEntry &entry : buf_pool) {
			if (entry.last_used_at < firstUse && buf_desc_compatible(entry.desc, desc)) {
				if ((match == nullptr) || entry.last_used_at > match->last_used_at) {
					match = &entry;
				}
			}
		}

		if (match != nullptr) {
			match->last_used_at = last;
			registry.resolve_buffer(RGBufferHandle{ b_id }, match->buf.get());
		} else {
			RHI::BufferDesc rhi_desc{};
			rhi_desc.size = desc.size;
			rhi_desc.usage = desc.usage;
			rhi_desc.domain = desc.domain;
			rhi_desc.debug_name = desc.debug_name.empty() ? "RG_Transient" : std::string(desc.debug_name);

			Ref<GFX::GfxBuffer> new_buf = ctx.create_buffer(rhi_desc);
			AQUILA_ASSERT(new_buf, "Failed to allocate transient buffer");

			out.transient_buffers.push_back(new_buf);
			registry.resolve_buffer(RGBufferHandle{ b_id }, new_buf.get());

			buf_pool.push_back(BufPoolEntry{ .buf = new_buf, .last_used_at = last, .desc = desc });
		}
	}
}

// Barrier inference
// Walk the sorted, culled pass list.  Per-resource state is tracked in flat
// arrays indexed by slot.  Whenever required state != current state, push a
// barrier record into the flat table and update current state.

void RGCompiler::infer_barriers(const std::vector<RGPassData> &passes, const std::vector<Uint32> &sorted_order,
								const std::vector<bool> &alive, Uint32 tex_count, Uint32 buf_count,
								const RGRegistry &registry, RGCompiledGraph &out) {
	// Imported resources start from their declared initial state, not Undefined,
	// so persistent textures don't get their contents discarded on first use.
	std::vector<RHI::ResourceState> cur_tex_state(tex_count, RHI::ResourceState::Undefined);
	for (Uint32 slot = 0; slot < tex_count; ++slot) {
		if (registry.is_imported_texture(RGTextureHandle{ slot })) {
			cur_tex_state[slot] = registry.get_texture_initial_state(RGTextureHandle{ slot });
		}
	}

	std::vector<RHI::ResourceState> cur_buf_state(buf_count, RHI::ResourceState::Undefined);
	for (Uint32 slot = 0; slot < buf_count; ++slot) {
		if (registry.is_imported_buffer(RGBufferHandle{ slot })) {
			cur_buf_state[slot] = registry.get_buffer_initial_state(RGBufferHandle{ slot });
		}
	}

	const Uint32 alive_count = static_cast<Uint32>(std::ranges::count(alive, true));

	// texBarriers is a flat list of ALL barriers jammed together
	out.tex_barriers.reserve(alive_count * 2);
	out.buf_barriers.reserve(alive_count);

	// to keep track and to know which barriers belong to which pass
	// for example : for pass N, we have M barriers
	//  texBarriers[ passTexBarStart[N] ... passTexBarStart[N+1] ]
	out.pass_tex_bar_start.reserve(alive_count + 1); // +1 for the sentinel
	out.pass_buf_bar_start.reserve(alive_count + 1); // same as before

	for (const Uint32 pass_index : sorted_order) {
		if (!alive[pass_index]) {
			continue;
		}
		const RGPassData &pass = passes[pass_index];

		// so if my texBarriers at this point is of size N, then the index table passTexBarStart begins at index N in the texBarriers vector
		// when the pass runs and pushes its barriers into texBarriers,
		// they naturally land at index N, N+1, etc.
		// until the next pass comes along and starts recording its own start position
		out.pass_tex_bar_start.push_back(static_cast<Uint32>(out.tex_barriers.size()));
		out.pass_buf_bar_start.push_back(static_cast<Uint32>(out.buf_barriers.size()));

		// Helper: emit a texture barrier if state changed
		auto maybe_texture_barrier = [&](RGTextureHandle handle, RHI::ResourceState required) {
			const Uint32 slot = slot_of(handle.id);

			// if textures current state is not what the next pass needs
			if (cur_tex_state[slot] != required) {
				// record a barrier from current state to required state
				out.tex_barriers.push_back({ handle, cur_tex_state[slot], required });

				// update current state
				cur_tex_state[slot] = required;
			}
		};

		// the same as before just for buffers
		auto maybe_buffer_barrier = [&](RGBufferHandle handle, RHI::ResourceState required) {
			const Uint32 slot = slot_of(handle.id);
			if (cur_buf_state[slot] != required) {
				out.buf_barriers.push_back({ handle, cur_buf_state[slot], required });
				cur_buf_state[slot] = required;
			}
		};

		for (const RGTextureAccess &texture_read : pass.texture_reads) {
			maybe_texture_barrier(texture_read.handle, texture_read.state);
		}
		for (const RGTextureAccess &texture_write : pass.texture_writes) {
			maybe_texture_barrier(texture_write.handle, texture_write.state);
		}
		for (const RGBufferAccess &buffer_read : pass.buffer_reads) {
			maybe_buffer_barrier(buffer_read.handle, buffer_read.state);
		}
		for (const RGBufferAccess &buffer_write : pass.buffer_writes) {
			maybe_buffer_barrier(buffer_write.handle, buffer_write.state);
		}
	}

	// a dummy end marker so that we can access barriers for the very last pass
	//
	// without dummy
	//  texBarriers     = [B0, B1, B2, B3, B4]
	//  passTexBarStart = [0, 2, 4]
	// with dummy
	//  passTexBarStart = [0, 2, 4, 5]

	out.pass_tex_bar_start.push_back(static_cast<Uint32>(out.tex_barriers.size()));
	out.pass_buf_bar_start.push_back(static_cast<Uint32>(out.buf_barriers.size()));
}

void RGCompiler::create_render_passes(const std::vector<RGPassData> &passes, const std::vector<Uint32> &sorted_order,
									  const std::vector<bool> &alive, const RGRegistry &registry, GFX::GfxContext &ctx,
									  RGCompiledGraph &out) {
	// passRenderPasses is indexed by position in passOrder (alive passes only)
	out.pass_render_passes.resize(out.pass_order.size());

	Uint32 sched_pos = 0;
	for (const Uint32 pass_index : sorted_order) {
		if (!alive[pass_index]) {
			continue;
		}
		const RGPassData &pass = passes[pass_index];

		const bool has_color = !pass.color_attachments.empty();
		const bool has_depth = pass.has_depth_attachment;

		if (!has_color && !has_depth) {
			++sched_pos;
			continue;
		} // Compute / copy pass

		RHI::RenderPassDesc rp_desc{};
		rp_desc.debug_name = pass.name;
		rp_desc.external_barriers = true; // Graph owns all barriers

		// Color attachments
		rp_desc.color_attachments.reserve(pass.color_attachments.size());
		for (const RGColorAttachment &rga : pass.color_attachments) {
			if (!rga.handle.is_valid()) {
				continue;
			}

			GFX::GfxTexture &tex = registry.get_texture(rga.handle);

			RHI::RenderPassColorAttachmentDesc color_attachment{};
			color_attachment.texture = &tex.get_rhi();
			color_attachment.clear_color = { rga.clear.color.r, rga.clear.color.g, rga.clear.color.b,
											 rga.clear.color.a };
			color_attachment.load_op = rga.load_op;
			color_attachment.store_op = rga.store_op;

			rp_desc.color_attachments.push_back(color_attachment);
		}

		// Depth attachment
		if (has_depth) {
			const RGDepthAttachment &rda = pass.depth_attachment;
			GFX::GfxTexture &tex = registry.get_texture(rda.handle);

			RHI::RenderPassDepthAttachmentDesc depth_attachment{};
			depth_attachment.texture = &tex.get_rhi();
			depth_attachment.depth_load_op = rda.depth_load_op;
			depth_attachment.depth_store_op = rda.depth_store_op;
			depth_attachment.stencil_load_op = rda.stencil_load_op;
			depth_attachment.stencil_store_op = rda.stencil_store_op;
			depth_attachment.read_only = rda.read_only;
			depth_attachment.clear_depth = rda.clear.depth;
			depth_attachment.clear_stencil = rda.clear.stencil;

			rp_desc.depth_attachment = depth_attachment;
		}

		// Derive dimensions from the first valid attachment
		if (!rp_desc.color_attachments.empty() && (rp_desc.color_attachments[0].texture != nullptr)) {
			rp_desc.width = rp_desc.color_attachments[0].texture->get_width();
			rp_desc.height = rp_desc.color_attachments[0].texture->get_height();
		} else if (has_depth && rp_desc.depth_attachment) {
			rp_desc.width = rp_desc.depth_attachment->texture->get_width();
			rp_desc.height = rp_desc.depth_attachment->texture->get_height();
		}

		out.pass_render_passes[sched_pos] = ctx.create_render_pass(rp_desc);
		++sched_pos;
	}
}

// Public entry point
RGCompiledGraph RGCompiler::compile(const std::vector<RGPassData> &passes, RGRegistry &registry, GFX::GfxContext &ctx) {
	RGCompiledGraph out;

	if (passes.empty()) {
		out.valid = true;
		return out;
	}

	const auto pass_count = static_cast<Uint32>(passes.size());
	const Uint32 tex_count = registry.texture_count();
	const Uint32 buf_count = registry.buffer_count();

	AdjList adj = build_dependency_graph(passes, tex_count, buf_count);

	std::vector<Uint32> sorted_order;
	std::vector<Uint32> cycle_path;
	const bool acyclic = topological_sort(adj, pass_count, sorted_order, cycle_path);

	if (!acyclic) {
		AQUILA_LOG_CRITICAL("Rendergraph cycle detected involving passes : ");
		for (Uint32 pass_index : cycle_path) {
			AQUILA_LOG_CRITICAL("   {}", passes[pass_index].name);
		}
		return out;
	}

	std::vector<bool> alive = cull_passes(passes, adj, sorted_order, registry);

	// Build final live-only pass order
	out.pass_order.reserve(pass_count);
	for (Uint32 pass_index : sorted_order) {
		if (alive[pass_index]) {
			out.pass_order.push_back(pass_index);
		}
	}

	std::vector<LifetimeInterval> tex_lifetimes =
		compute_tex_lifetimes(passes, sorted_order, alive, tex_count, registry);
	std::vector<LifetimeInterval> buf_lifetimes =
		compute_buf_lifetimes(passes, sorted_order, alive, buf_count, registry);

	allocate_transients(passes, registry, tex_lifetimes, buf_lifetimes, ctx, out);

	infer_barriers(passes, sorted_order, alive, tex_count, buf_count, registry, out);

	create_render_passes(passes, sorted_order, alive, registry, ctx, out);

	out.valid = true;
	return out;
}

} // namespace Aquila::Graphics::RG
