#pragma once
#include "Aquila/Foundation/Defines.h"
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/GFX/GfxContext.h"
#include "Aquila/GFX/GfxCommandList.h"
#include "Aquila/GFX/GfxBuffer.h"
#include "Aquila/GFX/GfxTexture.h"
#include "Aquila/GFX/GfxPipeline.h"
#include "Aquila/GFX/GfxDescriptorSet.h"
#include "Aquila/RHI/Backend/RHITypes.h"

namespace Aquila::Graphics {

struct QuadVertex {
	Vec3 position = DEFAULT_VALUE;
	Uint32 glyph_id = DEFAULT_VALUE;
	Vec4 color = DEFAULT_VALUE;
	Vec2 uv = DEFAULT_VALUE;
	Vec2 size = DEFAULT_VALUE;
	Vec4 radius = DEFAULT_VALUE;
	float border_width = DEFAULT_VALUE;
	float border_style = DEFAULT_VALUE;
	float pad1[2]{};
	Vec4 border_color = DEFAULT_VALUE;
};

struct Vertex {
	Vec2 position = DEFAULT_VALUE;
	Vec4 color = DEFAULT_VALUE;
	F32 size = DEFAULT_VALUE;
};

struct QuadPushConstants {
	Mat4 view_projection;
};

struct RectSpec {
	Vec2 position = { 0.F, 0.F };
	Vec2 size = { 1.F, 1.F };
	Vec4 color = { 1.F, 1.F, 1.F, 1.F };
	float rotation = 0.F;
	float depth = 0.F;
	Vec4 radius = { 0.F, 0.F, 0.F, 0.F };
	float border_width = 0.F;
	Vec4 border_color = { 0.F, 0.F, 0.F, 0.F };
	float border_style = 0.F;
};

struct PointSpec {
	Vec2 position = { 0.F, 0.F };
	Vec4 color = { 1.F, 1.F, 1.F, 1.F };
	F32 size = 1.F;
};

struct SpriteSpec {
	Vec2 position = { 0.F, 0.F };
	Vec2 size = { 1.F, 1.F };
	Vec4 tint = { 1.F, 1.F, 1.F, 1.F };
	float rotation = 0.F;
	float depth = 0.F;
	GFX::GfxTexture *texture = nullptr;
	Vec2 uv_min = { 0.F, 0.F };
	Vec2 uv_max = { 1.F, 1.F };
};

// Vertex format used exclusively by the text (Slug) pipeline.
// banding and glyphData are declared nointerpolation in the shader;
// all 4 vertices of a glyph quad carry the same values for those fields.
struct TextVertex {
	Vec3 position; // screen-space XYZ (Z = depth)
	float pad0{};
	Vec4 color;
	Vec2 texcoord; // em-space coordinate (interpolated)
	float tex_loc{}; // bit_cast<float>(glyphLocX | (glyphLocY << 16))
	float band_max{}; // bit_cast<float>((bandMaxX & 0xFF) | (bandMaxY << 8))
	Vec4 banding; // (scaleX, scaleY, offsetX, offsetY) nointerpolation
};
static_assert(sizeof(TextVertex) == 64);

struct ShadowSpec {
	Vec2 position = { 0.F, 0.F }; // top-left of the expanded shadow quad
	Vec2 size = { 1.F, 1.F }; // shadow quad size = widgetSize + 2*(blur+spread)
	Vec4 color = { 0.F, 0.F, 0.F, 0.75F };
	Vec2 offset = { 0.F, 0.F }; // CSS shadow-offset (x, y)
	Vec2 original_half_size = { 0.F, 0.F }; // widgetHalfSize + spread (SDF box)
	Vec4 radius = { 0.F, 0.F, 0.F, 0.F };
	float blur = 0.F;
	float depth = 0.F;
};

struct GlyphSpec {
	Vec2 position = { 0.F, 0.F }; // top-left screen position
	Vec2 size = { 1.F, 1.F }; // screen size
	Vec4 color = { 1.F, 1.F, 1.F, 1.F };
	float depth = 0.F;
	// Slug per-glyph data (constant across all 4 vertices).
	Uint32 glyph_loc_x = 0;
	Uint32 glyph_loc_y = 0;
	Uint32 band_max_x = 15;
	Uint32 band_max_y = 15;
	Vec4 banding = { 0.F, 0.F, 0.F, 0.F };
	// Em-space extents for computing per-corner texcoords.
	Vec2 em_min = { 0.F, 0.F };
	Vec2 em_max = { 1.F, 1.F };
	GFX::GfxTexture *curve_texture = nullptr;
	GFX::GfxTexture *band_texture = nullptr;
	bool flip_y = false;
};

/// Batches 2D rects, sprites, glyphs, shadows and points into as few draws as it can.
/// Draw between begin() and end(), inside a render pass.
///
/// @code
/// r2d.begin(cmd, RHI::TextureFormat::BGRA8, RHI::SampleCount::X1, ortho);
/// r2d.draw_rect({ .position = { 10.F, 10.F }, .size = { 120.F, 32.F }, .color = { 1.F, 0.F, 0.F, 1.F } });
/// r2d.end();
/// @endcode
class Renderer2D {
  public:
	explicit Renderer2D(GFX::GfxContext &ctx);
	~Renderer2D();
	AQUILA_NONCOPYABLE(Renderer2D);
	AQUILA_NONMOVEABLE(Renderer2D);

	void begin(GFX::GfxCommandList &cmd, RHI::TextureFormat color_format, RHI::SampleCount sample_count,
			   const Mat4 &view_projection, RHI::TextureFormat depth_format = RHI::TextureFormat::None);
	/// Draws what's batched so far. end() and batch switches call it for you.
	void flush();
	void end();

	/// Call before begin() to record that frame. execute_replay() then draws the same thing again
	/// without rebuilding any vertices, the UI does it when the canvas didn't change.
	void begin_capture();
	void execute_replay(GFX::GfxCommandList &cmd);
	/// Use this, not cmd.set_scissor(), so the scissor ends up in a capture too.
	void set_scissor(GFX::GfxCommandList &cmd, Int32 x, Int32 y, Uint32 w, Uint32 h);

	void draw_rect(const RectSpec &spec);
	void draw_shadow(const ShadowSpec &spec);
	void draw_sprite(const SpriteSpec &spec);
	void draw_glyph(const GlyphSpec &spec);
	void draw_point(const PointSpec &spec);

	void reset_stats() { m_stats = {}; }
	[[nodiscard]] Uint32 get_draw_call_count() const { return m_stats.draw_calls; }
	[[nodiscard]] Uint32 get_quad_count() const { return m_stats.quad_count; }
	[[nodiscard]] Uint32 get_primitive_count() const { return m_stats.primitive_count; }

	/// The get_or_create_*_pipeline functions cache per color format, sample count and depth format.
	GFX::GfxPipeline &get_or_create_flat_pipeline(RHI::TextureFormat color_format, RHI::SampleCount samples,
												  RHI::TextureFormat depth_format);
	GFX::GfxPipeline &get_or_create_texture_pipeline(RHI::TextureFormat color_format, RHI::SampleCount samples,
													 RHI::TextureFormat depth_format);
	GFX::GfxPipeline &get_or_create_gui_pipeline(RHI::TextureFormat color_format, RHI::SampleCount samples,
												 RHI::TextureFormat depth_format);
	GFX::GfxPipeline &get_or_create_text_pipeline(RHI::TextureFormat color_format, RHI::SampleCount samples,
												  RHI::TextureFormat depth_format);
	GFX::GfxPipeline &get_or_create_shadow_pipeline(RHI::TextureFormat color_format, RHI::SampleCount samples,
													RHI::TextureFormat depth_format);
	GFX::GfxPipeline &get_or_create_point_pipeline(RHI::TextureFormat color_format, RHI::SampleCount samples,
												   RHI::TextureFormat depth_format);

  private:
	struct Stats {
		Uint32 draw_calls = 0;
		Uint32 quad_count = 0;
		Uint32 primitive_count = 0;
	};

	struct PipelineKey {
		RHI::TextureFormat color_format;
		RHI::SampleCount sample_count;
		RHI::TextureFormat depth_format;
		bool operator==(const PipelineKey &o) const {
			return color_format == o.color_format && sample_count == o.sample_count && depth_format == o.depth_format;
		}
	};
	struct PipelineKeyHash {
		size_t operator()(const PipelineKey &k) const {
			size_t h = std::hash<int>{}(static_cast<int>(k.color_format));
			h ^= std::hash<int>{}(static_cast<int>(k.sample_count)) << 16;
			h ^= std::hash<int>{}(static_cast<int>(k.depth_format)) << 24;
			return h;
		}
	};

	using PipelineCache = std::unordered_map<PipelineKey, Ref<GFX::GfxPipeline>, PipelineKeyHash>;

	enum class BatchType : Uint8 { Flat, Point, GUI, Texture, Text, Shadow };

	enum class VertexStream : Uint8 { Quad, Text, Primitive };

	struct ReplayEntry {
		GFX::GfxPipeline *pipeline{};
		GFX::GfxDescriptorSet *desc_set{}; // null for pipelines with no descriptor set
		VertexStream stream = VertexStream::Quad;
		bool indexed = true;
		Uint32 count{};
		Int32 vertex_offset{};
		bool is_scissor = false;
		Int32 scissor_x = 0;
		Int32 scissor_y = 0;
		Uint32 scissor_w = 0;
		Uint32 scissor_h = 0;
	};

	void start_batch();
	void register_shader_hot_reload();
	[[nodiscard]] Mat4 build_quad_transform(Vec2 position, Vec2 size, float rotation, float depth) const;
	GFX::GfxPipeline &get_or_create_pipeline(PipelineCache &cache, std::string_view shader_file,
											 GFX::GfxDescriptorSetLayout *set_layout,
											 RHI::VertexBindingDesc (*vertex_layout)(), RHI::PrimitiveTopology topology,
											 const PipelineKey &key);
	GFX::GfxDescriptorSet &get_or_create_texture_set(GFX::GfxTexture &texture);
	GFX::GfxDescriptorSet &get_or_create_text_data_set(GFX::GfxTexture &curve_texture, GFX::GfxTexture &band_texture);

	GFX::GfxContext &m_ctx;

	// Per-frame ring buffers — cycle between two slots to avoid GPU/CPU sync stalls.
	static constexpr Uint32 K_RING_SIZE = SharedConstants::MAX_FRAMES_IN_FLIGHT;
	std::array<Ref<GFX::GfxBuffer>, K_RING_SIZE> m_vertex_buffers;
	std::array<Ref<GFX::GfxBuffer>, K_RING_SIZE> m_text_vertex_buffers;
	std::array<Ref<GFX::GfxBuffer>, K_RING_SIZE> m_primitive_vertex_buffers;
	QuadVertex *m_mapped_quad_bases[K_RING_SIZE] = {};
	TextVertex *m_mapped_text_bases[K_RING_SIZE] = {};
	Vertex *m_mapped_primitive_bases[K_RING_SIZE] = {};
	GFX::GfxBuffer *m_active_vertex_buffer = nullptr;
	GFX::GfxBuffer *m_active_text_vertex_buffer = nullptr;
	GFX::GfxBuffer *m_active_primitive_vertex_buffer = nullptr;
	// Direct write pointers into the currently active mapped buffer — no intermediate vector.
	QuadVertex *m_quad_write_ptr = nullptr;
	TextVertex *m_text_write_ptr = nullptr;
	Vertex *m_vertex_write_ptr = nullptr;
	Uint32 m_frame_counter = 0;

	Ref<GFX::GfxBuffer> m_index_buffer;
	Ref<GFX::GfxDescriptorSetLayout> m_texture_layout;
	Ref<GFX::GfxDescriptorSetLayout> m_text_data_layout;
	std::unordered_map<GFX::GfxTexture *, Ref<GFX::GfxDescriptorSet>> m_texture_set_cache;
	std::unordered_map<GFX::GfxTexture *, Ref<GFX::GfxDescriptorSet>> m_text_data_set_cache;

	PipelineCache m_flat_pipelines;
	PipelineCache m_texture_pipelines;
	PipelineCache m_gui_pipelines;
	PipelineCache m_point_pipelines;
	PipelineCache m_text_pipelines;
	PipelineCache m_shadow_pipelines;

	std::vector<Uint64> m_watch_ids;

	GFX::GfxCommandList *m_active_cmd = nullptr;
	RHI::TextureFormat m_active_color_format = RHI::TextureFormat::None;
	RHI::TextureFormat m_active_depth_format = RHI::TextureFormat::None;
	RHI::SampleCount m_active_sample_count = RHI::SampleCount::X1;
	Mat4 m_view_projection = Mat4(1.F);
	// Per-frame command recording state — avoids redundant GPU state changes.
	GFX::GfxPipeline *m_last_bound_pipeline = nullptr;
	GFX::GfxDescriptorSet *m_last_bound_desc_set0 = nullptr;
	GFX::GfxBuffer *m_last_bound_vertex_buffer = nullptr;
	bool m_push_constants_dirty = true;
	// Cached descriptor set pointers — updated when the batch texture changes, not per-flush.
	GFX::GfxDescriptorSet *m_cached_text_data_set = nullptr;
	GFX::GfxDescriptorSet *m_cached_texture_set = nullptr;

	Uint32 m_quad_count = 0;
	Uint32 m_vertex_offset = 0;
	Uint32 m_text_vertex_offset = 0;
	Uint32 m_primitive_vertex_offset = 0;
	Uint32 m_primitive_batch_count = 0;
	BatchType m_batch_type = BatchType::Flat;
	GFX::GfxTexture *m_batch_texture = nullptr;
	GFX::GfxTexture *m_batch_curve_texture = nullptr;
	GFX::GfxTexture *m_batch_band_texture = nullptr;

	// Replay state — built during a captured Submit, consumed by ExecuteReplay.
	std::vector<ReplayEntry> m_replay_list;
	bool m_capturing = false;
	Uint32 m_current_slot = 0; // ring slot in use this frame (set by Begin)
	Uint32 m_last_dirty_slot = 0; // ring slot written during last BeginCapture+Submit
	Uint64 m_replay_quad_bytes = 0;
	Uint64 m_replay_text_bytes = 0;

	Stats m_stats;
};

} // namespace Aquila::Graphics
