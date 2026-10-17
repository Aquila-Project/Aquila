#ifndef AQUILA_RHI_TYPES_H
#define AQUILA_RHI_TYPES_H

#include "Aquila/Foundation/Defines.h"
#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Aquila::RHI {

class IRHIDescriptorSetLayout;
class IRHITexture;
class IRHISwapchain;

// NOTES:
// USE GPU_ONLY FOR GPU LOCAL BUFFERS - vertex, index, storage
// USE CPU_TO_GPU FOR SEQUENTIAL WRITES - uniform buffers or staging buffers
// USE GPU_TO_CPU FOR RANDOM ACCESS - readbacks
enum class MemoryDomain : Uint8 { GpuOnly, CpuOnly, GpuToCpu, CpuToGpu };

enum class TextureFormat : Uint8 {
	None = 0,
	RGBA8,
	RgbA8Srgb,
	RGBA16F,
	RGBA32F,
	RGBA32U,
	RGB8,
	RGB16F,
	RGB32F,
	RG8,
	RG16F,
	RG32F,
	R8,
	R16F,
	R32F,
	R32UI,
	BGRA8,
	BgrA8Srgb,
	Depth16,
	Depth32,
	Depth24Stencil8,
	Depth32Stencil8,
};

// Bitmask so usages can be combined
enum class TextureUsage : Uint8 {
	None = 0,
	ColorAttachment = BIT(0),
	DepthAttachment = BIT(1),
	Sampled = BIT(2),
	Storage = BIT(3),
	TransferSrc = BIT(4),
	TransferDst = BIT(5),
	InputAttachment = BIT(6),
};

AQUILA_FORCE_INLINE TextureUsage operator|(TextureUsage a, TextureUsage b) {
	return static_cast<TextureUsage>(static_cast<Uint32>(a) | static_cast<Uint32>(b));
}
AQUILA_FORCE_INLINE TextureUsage operator&(TextureUsage a, TextureUsage b) {
	return static_cast<TextureUsage>(static_cast<Uint32>(a) & static_cast<Uint32>(b));
}

enum class SampleCount : Uint8 { X1, X2, X4, X8, X16, X32, X64 };

enum class ShaderStageFlags : Uint8 {
	None = 0,
	Vertex = 1 << 0,
	Fragment = 1 << 1,
	Compute = 1 << 2,
	Geometry = 1 << 3,
	All = Vertex | Fragment | Compute | Geometry,
};

AQUILA_FORCE_INLINE ShaderStageFlags operator|(ShaderStageFlags a, ShaderStageFlags b) {
	return static_cast<ShaderStageFlags>(static_cast<Uint32>(a) | static_cast<Uint32>(b));
}
AQUILA_FORCE_INLINE ShaderStageFlags operator&(ShaderStageFlags a, ShaderStageFlags b) {
	return static_cast<ShaderStageFlags>(static_cast<Uint32>(a) & static_cast<Uint32>(b));
}

enum class FilterMode : Uint8 { Nearest, Linear };
enum class MipmapMode : Uint8 { Nearest, Linear };

enum class AddressMode : Uint8 {
	Repeat,
	MirroredRepeat,
	ClampToEdge,
	ClampToBorder,
};

enum class BorderColor : Uint8 {
	TransparentBlack,
	OpaqueBlack,
	OpaqueWhite,
};

enum class CompareOp : Uint8 {
	Never,
	Less,
	Equal,
	LessEqual,
	Greater,
	NotEqual,
	GreaterEqual,
	Always,
};

enum class CommandListType : Uint8 { Graphics, Compute, Transfer };

enum class BufferUsage : Uint32 {
	None = 0,
	VertexBuffer = BIT(0),
	IndexBuffer = BIT(1),
	UniformBuffer = BIT(2),
	StorageBuffer = BIT(3),
	TransferSrc = BIT(4),
	TransferDst = BIT(5),
	IndirectBuffer = BIT(6),
};

AQUILA_FORCE_INLINE BufferUsage operator|(BufferUsage a, BufferUsage b) {
	return static_cast<BufferUsage>(static_cast<Uint32>(a) | static_cast<Uint32>(b));
}
AQUILA_FORCE_INLINE BufferUsage operator&(BufferUsage a, BufferUsage b) {
	return static_cast<BufferUsage>(static_cast<Uint32>(a) & static_cast<Uint32>(b));
}

enum class IndexFormat : Uint8 { UInt16, UInt32 };

enum class TextureViewType : Uint8 {
	Tex1D,
	Tex2D,
	Tex3D,
	Cube,
	Tex1DArray,
	Tex2DArray,
	CubeArray,
};

enum class ComponentSwizzle : Uint8 { Identity, Zero, One, R, G, B, A };

struct ComponentMapping {
	ComponentSwizzle r = ComponentSwizzle::Identity;
	ComponentSwizzle g = ComponentSwizzle::Identity;
	ComponentSwizzle b = ComponentSwizzle::Identity;
	ComponentSwizzle a = ComponentSwizzle::Identity;

	bool operator==(const ComponentMapping &) const = default;
};

struct SamplerDesc {
	FilterMode mag_filter = FilterMode::Linear;
	FilterMode min_filter = FilterMode::Linear;
	MipmapMode mipmap_mode = MipmapMode::Nearest;
	AddressMode address_u = AddressMode::ClampToEdge;
	AddressMode address_v = AddressMode::ClampToEdge;
	AddressMode address_w = AddressMode::ClampToEdge;
	BorderColor border_color = BorderColor::TransparentBlack;
	float min_lod = 0.0F;
	float max_lod = 0.0F;
	float mip_lod_bias = 0.0F;
	bool anisotropy = false;
	bool compare_enable = false;
	CompareOp compare_op = CompareOp::Always;

	bool operator==(const SamplerDesc &) const = default;

	static SamplerDesc texture2_d(float max_lod = 1000.0F) {
		SamplerDesc d{};
		d.address_u = AddressMode::Repeat;
		d.address_v = AddressMode::Repeat;
		d.address_w = AddressMode::Repeat;
		d.mipmap_mode = MipmapMode::Linear;
		d.max_lod = max_lod;
		d.anisotropy = true;
		return d;
	}

	static SamplerDesc render_target() {
		SamplerDesc d{};
		d.mipmap_mode = MipmapMode::Linear;
		return d;
	}

	/// Compare sampler. Outside the map counts as lit.
	static SamplerDesc shadow_map() {
		SamplerDesc d{};
		d.address_u = AddressMode::ClampToBorder;
		d.address_v = AddressMode::ClampToBorder;
		d.address_w = AddressMode::ClampToBorder;
		d.border_color = BorderColor::OpaqueWhite;
		d.compare_enable = true;
		d.compare_op = CompareOp::LessEqual;
		return d;
	}

	static SamplerDesc point_sample() {
		SamplerDesc d{};
		d.mag_filter = FilterMode::Nearest;
		d.min_filter = FilterMode::Nearest;
		d.mipmap_mode = MipmapMode::Nearest;
		d.address_u = AddressMode::ClampToEdge;
		d.address_v = AddressMode::ClampToEdge;
		d.address_w = AddressMode::ClampToEdge;
		d.anisotropy = false;
		return d;
	}

	/// Slight negative LOD bias so downscaled images stay sharp.
	static SamplerDesc ui_image(float max_lod) {
		SamplerDesc d{};
		d.mipmap_mode = MipmapMode::Linear;
		d.max_lod = max_lod;
		d.mip_lod_bias = -0.5F;
		return d;
	}

	static SamplerDesc font_atlas() {
		SamplerDesc d{};
		d.mag_filter = FilterMode::Linear;
		d.min_filter = FilterMode::Linear;

		d.mipmap_mode = MipmapMode::Nearest;

		d.address_u = AddressMode::ClampToEdge;
		d.address_v = AddressMode::ClampToEdge;
		d.address_w = AddressMode::ClampToEdge;

		d.anisotropy = false;
		d.min_lod = 0.0F;
		d.max_lod = 0.0F;

		return d;
	}
};

struct SamplerDescHash {
	size_t operator()(const SamplerDesc &d) const {
		size_t h = 0;
		auto combine = [&](auto v) {
			h ^= std::hash<Uint32>{}(static_cast<Uint32>(v)) + 0x9e3779b9 + (h << 6) + (h >> 2);
		};
		combine(d.mag_filter);
		combine(d.min_filter);
		combine(d.mipmap_mode);
		combine(d.address_u);
		combine(d.address_v);
		combine(d.address_w);
		combine(d.border_color);
		combine(d.compare_enable);
		combine(d.compare_op);
		return h;
	}
};

/// With instance_count > 1 every instance is padded to min_alignment, like dynamic uniform buffers need.
struct BufferDesc {
	Uint64 size = 0;
	BufferUsage usage = BufferUsage::None;
	MemoryDomain domain = MemoryDomain::GpuOnly;
	Uint32 instance_count = 1;
	Uint64 min_alignment = 0;
	std::string debug_name;
};

struct TextureDesc {
	Uint32 width = 1;
	Uint32 height = 1;
	Uint32 depth = 1;
	Uint32 mip_levels = 1;
	Uint32 array_layers = 1;
	TextureFormat format = TextureFormat::RGBA8;
	TextureUsage usage = TextureUsage::Sampled;
	SampleCount samples = SampleCount::X1;
	TextureViewType view_type = TextureViewType::Tex2D;
	ComponentMapping swizzle = {};
	SamplerDesc sampler = SamplerDesc::texture2_d();
	std::string debug_name = "Texture";
};

/// native_window_handle is the GLFWwindow of a secondary window, null for the main one.
struct SwapchainDesc {
	Uint32 width = 0;
	Uint32 height = 0;
	TextureFormat format = TextureFormat::BGRA8;
	Uint32 image_count = 2;
	bool vsync = true;
	void *native_window_handle = nullptr;
};

enum class PrimitiveTopology : Uint8 {
	TriangleList,
	TriangleStrip,
	TriangleFan,
	LineList,
	LineStrip,
	PointList,
};

enum class CullMode : Uint8 { None, Front, Back };
enum class FillMode : Uint8 { Solid, Wireframe };
enum class FrontFace : Uint8 { Clockwise, CounterClockwise };

enum class BlendFactor : Uint8 {
	Zero,
	One,
	SrcColor,
	OneMinusSrcColor,
	DstColor,
	OneMinusDstColor,
	SrcAlpha,
	OneMinusSrcAlpha,
	DstAlpha,
	OneMinusDstAlpha,
};

enum class BlendOp : Uint8 { Add, Subtract, ReverseSubtract, Min, Max };

enum class DescriptorType : Uint8 {
	UniformBuffer,
	StorageBuffer,
	CombinedImageSampler,
	StorageImage,
	InputAttachment,
};

struct VertexAttributeDesc {
	Uint32 location;
	Uint32 binding;
	TextureFormat format;
	Uint32 offset;
};

struct VertexBindingDesc {
	Uint32 stride;
	std::vector<VertexAttributeDesc> attributes;
};

struct ShaderStageDesc {
	ShaderStageFlags stage = ShaderStageFlags::Vertex;
	std::vector<Uint32> spirv;
	std::string entry_point = "main";
};

struct PushConstantRange {
	ShaderStageFlags stages = ShaderStageFlags::Vertex;
	Uint32 offset = 0;
	Uint32 size = 0;
};

struct BlendAttachmentDesc {
	bool enable = false;
	BlendFactor src_color = BlendFactor::SrcAlpha;
	BlendFactor dst_color = BlendFactor::OneMinusSrcAlpha;
	BlendOp color_op = BlendOp::Add;
	BlendFactor src_alpha = BlendFactor::One;
	BlendFactor dst_alpha = BlendFactor::Zero;
	BlendOp alpha_op = BlendOp::Add;
};

struct RasterStateDesc {
	CullMode cull_mode = CullMode::Back;
	FillMode fill_mode = FillMode::Solid;
	FrontFace front_face = FrontFace::CounterClockwise;
	bool depth_clamp = false;
	float line_width = 1.0F;
};

struct DepthStencilStateDesc {
	bool depth_test = true;
	bool depth_write = true;
	CompareOp depth_compare = CompareOp::Less;
	bool stencil_test = false;
};

struct GraphicsPipelineDesc {
	ShaderStageDesc vertex_shader;
	ShaderStageDesc fragment_shader;
	PrimitiveTopology topology = PrimitiveTopology::TriangleList;
	RasterStateDesc raster;
	DepthStencilStateDesc depth_stencil;
	std::vector<BlendAttachmentDesc> blend_attachments = { {} };
	std::vector<TextureFormat> color_formats;
	TextureFormat depth_format = TextureFormat::Depth32;
	SampleCount sample_count = SampleCount::X1;
	bool min_sample_shading = false;
	std::vector<IRHIDescriptorSetLayout *> set_layouts;
	std::vector<PushConstantRange> push_constants;
	std::optional<VertexBindingDesc> custom_vertex_layout;
	bool no_vertex_input = false; // true for shader-only draws (SV_VertexID, no VB)
	std::string debug_name;
};

struct ComputePipelineDesc {
	ShaderStageDesc compute_shader;
	std::vector<IRHIDescriptorSetLayout *> set_layouts;
	std::vector<PushConstantRange> push_constants;
	std::string debug_name;
};

struct DescriptorBinding {
	Uint32 binding = 0;
	DescriptorType type = DescriptorType::UniformBuffer;
	ShaderStageFlags stages = ShaderStageFlags::Vertex;
	Uint32 count = 1;
};

struct DescriptorSetLayoutDesc {
	std::vector<DescriptorBinding> bindings;
};

enum class AttachmentLoadOp : Uint8 { Load, Clear, DontCare };
enum class AttachmentStoreOp : Uint8 { Store, DontCare };

struct RenderPassColorAttachmentDesc {
	IRHITexture *texture = nullptr; // null = use swapchain image
	IRHITexture *resolve_texture = nullptr; // MSAA resolve target
	Vec4 clear_color = { 0.0F, 0.0F, 0.0F, 1.0F };
	AttachmentLoadOp load_op = AttachmentLoadOp::Clear;
	AttachmentStoreOp store_op = AttachmentStoreOp::Store;
	Uint32 mip_level = 0; // target mip level (for rendering into a mip)
	Uint32 array_layer = 0; // target array layer / cubemap face
};

struct RenderPassDepthAttachmentDesc {
	IRHITexture *texture = nullptr; // null = use swapchain depth
	float clear_depth = 1.0F;
	Uint8 clear_stencil = 0;
	AttachmentLoadOp depth_load_op = AttachmentLoadOp::Clear;
	AttachmentStoreOp depth_store_op = AttachmentStoreOp::DontCare;
	AttachmentLoadOp stencil_load_op = AttachmentLoadOp::DontCare;
	AttachmentStoreOp stencil_store_op = AttachmentStoreOp::DontCare;
	bool read_only = false; // depth test but no write (read-only depth attachment)
};

struct RenderPassDesc {
	std::vector<RenderPassColorAttachmentDesc> color_attachments{};
	std::optional<RenderPassDepthAttachmentDesc> depth_attachment{};
	bool use_swapchain = false;
	// When true, colorAttachments[0].texture is the MSAA render target and the
	// swapchain image (passed to Begin()) is used as the resolve destination.
	bool use_swapchain_as_resolve = false;
	Uint32 width = 0;
	Uint32 height = 0;

	std::string debug_name = "RenderPass";

	// When true, VulkanRenderPass::Begin/End will NOT emit its built-in pre/post
	// barriers.  Set by the RenderGraph compiler, which handles all transitions
	// through its own barrier system.
	bool external_barriers = false;
};

enum class ResourceState : Uint16 {
	Undefined = 0,
	ColorAttachmentRead = 1 << 0,
	ColorAttachmentWrite = 1 << 1,
	DepthStencilRead = 1 << 2,
	DepthStencilWrite = 1 << 3,
	ShaderRead = 1 << 4, // sampled / SRV
	StorageRead = 1 << 5, // UAV read
	StorageWrite = 1 << 6, // UAV write
	UniformRead = 1 << 7,
	TransferSrc = 1 << 8,
	TransferDst = 1 << 9,
	IndirectArgument = 1 << 10,
	IndexBuffer = 1 << 11,
	VertexBuffer = 1 << 12,
	Present = 1 << 13,
	HostRead = 1 << 14,
	ColorAttachment = ColorAttachmentRead | ColorAttachmentWrite,
	DepthRead = DepthStencilRead,
	DepthWrite = DepthStencilRead | DepthStencilWrite,
	UnorderedAccess = StorageRead | StorageWrite,

};

} // namespace Aquila::RHI
#endif
