#pragma once
#include "GraphicsPCH.h"
#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/RHI/Backend/RHITypes.h"
#include "Aquila/RHI/FormatUtils.h"

namespace Aquila::RHI {

AQUILA_FORCE_INLINE VkFormat to_vk_format(TextureFormat format) {
	switch (format) {
	case TextureFormat::RGBA8:
		return VK_FORMAT_R8G8B8A8_UNORM;
	case TextureFormat::RgbA8Srgb:
		return VK_FORMAT_R8G8B8A8_SRGB;
	case TextureFormat::RGBA16F:
		return VK_FORMAT_R16G16B16A16_SFLOAT;
	case TextureFormat::RGBA32F:
		return VK_FORMAT_R32G32B32A32_SFLOAT;
	case TextureFormat::RGBA32U:
		return VK_FORMAT_R32G32B32A32_UINT;
	case TextureFormat::RGB8:
		return VK_FORMAT_R8G8B8_UNORM;
	case TextureFormat::RGB16F:
		return VK_FORMAT_R16G16B16_SFLOAT;
	case TextureFormat::RGB32F:
		return VK_FORMAT_R32G32B32_SFLOAT;
	case TextureFormat::RG8:
		return VK_FORMAT_R8G8_UNORM;
	case TextureFormat::RG16F:
		return VK_FORMAT_R16G16_SFLOAT;
	case TextureFormat::RG32F:
		return VK_FORMAT_R32G32_SFLOAT;
	case TextureFormat::R8:
		return VK_FORMAT_R8_UNORM;
	case TextureFormat::R16F:
		return VK_FORMAT_R16_SFLOAT;
	case TextureFormat::R32F:
		return VK_FORMAT_R32_SFLOAT;
	case TextureFormat::R32UI:
		return VK_FORMAT_R32_UINT;
	case TextureFormat::BGRA8:
		return VK_FORMAT_B8G8R8A8_UNORM;
	case TextureFormat::BgrA8Srgb:
		return VK_FORMAT_B8G8R8A8_SRGB;
	case TextureFormat::Depth16:
		return VK_FORMAT_D16_UNORM;
	case TextureFormat::Depth32:
		return VK_FORMAT_D32_SFLOAT;
	case TextureFormat::Depth24Stencil8:
		return VK_FORMAT_D24_UNORM_S8_UINT;
	case TextureFormat::Depth32Stencil8:
		return VK_FORMAT_D32_SFLOAT_S8_UINT;
	default:
		AQUILA_ASSERT(false, "Unknown TextureFormat");
		return VK_FORMAT_UNDEFINED;
	}
}

AQUILA_FORCE_INLINE TextureFormat from_vk_format(VkFormat format) {
	switch (format) {
	case VK_FORMAT_R8G8B8A8_UNORM:
		return TextureFormat::RGBA8;
	case VK_FORMAT_R8G8B8A8_SRGB:
		return TextureFormat::RgbA8Srgb;
	case VK_FORMAT_R16G16B16A16_SFLOAT:
		return TextureFormat::RGBA16F;
	case VK_FORMAT_R32G32B32A32_SFLOAT:
		return TextureFormat::RGBA32F;
	case VK_FORMAT_R32G32B32A32_UINT:
		return TextureFormat::RGBA32U;
	case VK_FORMAT_R8G8B8_UNORM:
		return TextureFormat::RGB8;
	case VK_FORMAT_R16G16B16_SFLOAT:
		return TextureFormat::RGB16F;
	case VK_FORMAT_R32G32B32_SFLOAT:
		return TextureFormat::RGB32F;
	case VK_FORMAT_R8G8_UNORM:
		return TextureFormat::RG8;
	case VK_FORMAT_R16G16_SFLOAT:
		return TextureFormat::RG16F;
	case VK_FORMAT_R32G32_SFLOAT:
		return TextureFormat::RG32F;
	case VK_FORMAT_R8_UNORM:
		return TextureFormat::R8;
	case VK_FORMAT_R16_SFLOAT:
		return TextureFormat::R16F;
	case VK_FORMAT_R32_SFLOAT:
		return TextureFormat::R32F;
	case VK_FORMAT_R32_UINT:
		return TextureFormat::R32UI;
	case VK_FORMAT_B8G8R8A8_UNORM:
		return TextureFormat::BGRA8;
	case VK_FORMAT_B8G8R8A8_SRGB:
		return TextureFormat::BgrA8Srgb;
	case VK_FORMAT_D16_UNORM:
		return TextureFormat::Depth16;
	case VK_FORMAT_D32_SFLOAT:
		return TextureFormat::Depth32;
	case VK_FORMAT_D24_UNORM_S8_UINT:
		return TextureFormat::Depth24Stencil8;
	case VK_FORMAT_D32_SFLOAT_S8_UINT:
		return TextureFormat::Depth32Stencil8;
	default:
		AQUILA_ASSERT(false, "Unknown VkFormat");
		return TextureFormat::RGBA8;
	}
}

AQUILA_FORCE_INLINE VkSampleCountFlagBits to_vk_sample_count(SampleCount samples) {
	switch (samples) {
	case SampleCount::X1:
		return VK_SAMPLE_COUNT_1_BIT;
	case SampleCount::X2:
		return VK_SAMPLE_COUNT_2_BIT;
	case SampleCount::X4:
		return VK_SAMPLE_COUNT_4_BIT;
	case SampleCount::X8:
		return VK_SAMPLE_COUNT_8_BIT;
	case SampleCount::X16:
		return VK_SAMPLE_COUNT_16_BIT;
	case SampleCount::X32:
		return VK_SAMPLE_COUNT_32_BIT;
	case SampleCount::X64:
		return VK_SAMPLE_COUNT_64_BIT;
	default:
		AQUILA_ASSERT(false, "Unknown SampleCount");
		return VK_SAMPLE_COUNT_1_BIT;
	}
}

/// Every aspect the format has, what barriers want. A view you sample from takes the depth aspect alone.
AQUILA_FORCE_INLINE VkImageAspectFlags to_vk_image_aspect(TextureFormat format) {
	if (is_stencil_format(format)) {
		return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	}
	if (is_depth_format(format)) {
		return VK_IMAGE_ASPECT_DEPTH_BIT;
	}
	return VK_IMAGE_ASPECT_COLOR_BIT;
}

AQUILA_FORCE_INLINE VkImageUsageFlags to_vk_image_usage(TextureUsage usage) {
	VkImageUsageFlags flags = 0;
	if ((usage & TextureUsage::ColorAttachment) != TextureUsage::None) {
		flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	}
	if ((usage & TextureUsage::DepthAttachment) != TextureUsage::None) {
		flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	}
	if ((usage & TextureUsage::Sampled) != TextureUsage::None) {
		flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
	}
	if ((usage & TextureUsage::Storage) != TextureUsage::None) {
		flags |= VK_IMAGE_USAGE_STORAGE_BIT;
	}
	if ((usage & TextureUsage::TransferSrc) != TextureUsage::None) {
		flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	}
	if ((usage & TextureUsage::TransferDst) != TextureUsage::None) {
		flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}
	if ((usage & TextureUsage::InputAttachment) != TextureUsage::None) {
		flags |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
	}
	return flags;
}

AQUILA_FORCE_INLINE VkBufferUsageFlags to_vk_buffer_usage(BufferUsage usage) {
	VkBufferUsageFlags flags = 0;
	if ((usage & BufferUsage::VertexBuffer) != BufferUsage::None) {
		flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	}
	if ((usage & BufferUsage::IndexBuffer) != BufferUsage::None) {
		flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	}
	if ((usage & BufferUsage::UniformBuffer) != BufferUsage::None) {
		flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	}
	if ((usage & BufferUsage::StorageBuffer) != BufferUsage::None) {
		flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	}
	if ((usage & BufferUsage::TransferSrc) != BufferUsage::None) {
		flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	}
	if ((usage & BufferUsage::TransferDst) != BufferUsage::None) {
		flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	}
	if ((usage & BufferUsage::IndirectBuffer) != BufferUsage::None) {
		flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
	}
	return flags;
}

AQUILA_FORCE_INLINE VkImageViewType to_vk_image_view_type(TextureViewType type) {
	switch (type) {
	case TextureViewType::Tex1D:
		return VK_IMAGE_VIEW_TYPE_1D;
	case TextureViewType::Tex2D:
		return VK_IMAGE_VIEW_TYPE_2D;
	case TextureViewType::Tex3D:
		return VK_IMAGE_VIEW_TYPE_3D;
	case TextureViewType::Cube:
		return VK_IMAGE_VIEW_TYPE_CUBE;
	case TextureViewType::Tex1DArray:
		return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
	case TextureViewType::Tex2DArray:
		return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	case TextureViewType::CubeArray:
		return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
	}
	return VK_IMAGE_VIEW_TYPE_2D;
}

AQUILA_FORCE_INLINE VkComponentSwizzle to_vk_component_swizzle(ComponentSwizzle s) {
	switch (s) {
	case ComponentSwizzle::Identity:
		return VK_COMPONENT_SWIZZLE_IDENTITY;
	case ComponentSwizzle::Zero:
		return VK_COMPONENT_SWIZZLE_ZERO;
	case ComponentSwizzle::One:
		return VK_COMPONENT_SWIZZLE_ONE;
	case ComponentSwizzle::R:
		return VK_COMPONENT_SWIZZLE_R;
	case ComponentSwizzle::G:
		return VK_COMPONENT_SWIZZLE_G;
	case ComponentSwizzle::B:
		return VK_COMPONENT_SWIZZLE_B;
	case ComponentSwizzle::A:
		return VK_COMPONENT_SWIZZLE_A;
	}
	return VK_COMPONENT_SWIZZLE_IDENTITY;
}

AQUILA_FORCE_INLINE VkComponentMapping to_vk_component_mapping(const ComponentMapping &m) {
	return { to_vk_component_swizzle(m.r), to_vk_component_swizzle(m.g), to_vk_component_swizzle(m.b),
			 to_vk_component_swizzle(m.a) };
}

AQUILA_FORCE_INLINE VkShaderStageFlags to_vk_shader_stage(ShaderStageFlags flags) {
	VkShaderStageFlags vk = 0;
	if ((flags & ShaderStageFlags::Vertex) != ShaderStageFlags::None) {
		vk |= VK_SHADER_STAGE_VERTEX_BIT;
	}
	if ((flags & ShaderStageFlags::Fragment) != ShaderStageFlags::None) {
		vk |= VK_SHADER_STAGE_FRAGMENT_BIT;
	}
	if ((flags & ShaderStageFlags::Compute) != ShaderStageFlags::None) {
		vk |= VK_SHADER_STAGE_COMPUTE_BIT;
	}
	if ((flags & ShaderStageFlags::Geometry) != ShaderStageFlags::None) {
		vk |= VK_SHADER_STAGE_GEOMETRY_BIT;
	}
	return vk;
}

AQUILA_FORCE_INLINE VkPrimitiveTopology to_vk_primitive_topology(PrimitiveTopology t) {
	switch (t) {
	case PrimitiveTopology::TriangleList:
		return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	case PrimitiveTopology::TriangleStrip:
		return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	case PrimitiveTopology::TriangleFan:
		return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
	case PrimitiveTopology::LineList:
		return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
	case PrimitiveTopology::LineStrip:
		return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
	case PrimitiveTopology::PointList:
		return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
	}
	return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
}

AQUILA_FORCE_INLINE VkCullModeFlags to_vk_cull_mode(CullMode m) {
	switch (m) {
	case CullMode::None:
		return VK_CULL_MODE_NONE;
	case CullMode::Front:
		return VK_CULL_MODE_FRONT_BIT;
	case CullMode::Back:
		return VK_CULL_MODE_BACK_BIT;
	}
	return VK_CULL_MODE_BACK_BIT;
}

AQUILA_FORCE_INLINE VkPolygonMode to_vk_polygon_mode(FillMode m) {
	return m == FillMode::Wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
}

AQUILA_FORCE_INLINE VkFrontFace to_vk_front_face(FrontFace f) {
	return f == FrontFace::Clockwise ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;
}

AQUILA_FORCE_INLINE VkBlendFactor to_vk_blend_factor(BlendFactor f) {
	switch (f) {
	case BlendFactor::Zero:
		return VK_BLEND_FACTOR_ZERO;
	case BlendFactor::One:
		return VK_BLEND_FACTOR_ONE;
	case BlendFactor::SrcColor:
		return VK_BLEND_FACTOR_SRC_COLOR;
	case BlendFactor::OneMinusSrcColor:
		return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
	case BlendFactor::DstColor:
		return VK_BLEND_FACTOR_DST_COLOR;
	case BlendFactor::OneMinusDstColor:
		return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
	case BlendFactor::SrcAlpha:
		return VK_BLEND_FACTOR_SRC_ALPHA;
	case BlendFactor::OneMinusSrcAlpha:
		return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	case BlendFactor::DstAlpha:
		return VK_BLEND_FACTOR_DST_ALPHA;
	case BlendFactor::OneMinusDstAlpha:
		return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
	}
	return VK_BLEND_FACTOR_ONE;
}

AQUILA_FORCE_INLINE VkBlendOp to_vk_blend_op(BlendOp op) {
	switch (op) {
	case BlendOp::Add:
		return VK_BLEND_OP_ADD;
	case BlendOp::Subtract:
		return VK_BLEND_OP_SUBTRACT;
	case BlendOp::ReverseSubtract:
		return VK_BLEND_OP_REVERSE_SUBTRACT;
	case BlendOp::Min:
		return VK_BLEND_OP_MIN;
	case BlendOp::Max:
		return VK_BLEND_OP_MAX;
	}
	return VK_BLEND_OP_ADD;
}

AQUILA_FORCE_INLINE VkDescriptorType to_vk_descriptor_type(DescriptorType t) {
	switch (t) {
	case DescriptorType::UniformBuffer:
		return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	case DescriptorType::StorageBuffer:
		return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	case DescriptorType::CombinedImageSampler:
		return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	case DescriptorType::StorageImage:
		return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	case DescriptorType::InputAttachment:
		return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	}
	return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
}

AQUILA_FORCE_INLINE VkCompareOp to_vk_compare_op(CompareOp op) {
	switch (op) {
	case CompareOp::Never:
		return VK_COMPARE_OP_NEVER;
	case CompareOp::Less:
		return VK_COMPARE_OP_LESS;
	case CompareOp::Equal:
		return VK_COMPARE_OP_EQUAL;
	case CompareOp::LessEqual:
		return VK_COMPARE_OP_LESS_OR_EQUAL;
	case CompareOp::Greater:
		return VK_COMPARE_OP_GREATER;
	case CompareOp::NotEqual:
		return VK_COMPARE_OP_NOT_EQUAL;
	case CompareOp::GreaterEqual:
		return VK_COMPARE_OP_GREATER_OR_EQUAL;
	case CompareOp::Always:
		return VK_COMPARE_OP_ALWAYS;
	}
	return VK_COMPARE_OP_LESS;
}

AQUILA_FORCE_INLINE VkFilter to_vk_filter(FilterMode mode) {
	switch (mode) {
	case FilterMode::Nearest:
		return VK_FILTER_NEAREST;
	case FilterMode::Linear:
		return VK_FILTER_LINEAR;
	}
	return VK_FILTER_LINEAR;
}

AQUILA_FORCE_INLINE VkSamplerMipmapMode to_vk_mipmap_mode(MipmapMode mode) {
	switch (mode) {
	case MipmapMode::Nearest:
		return VK_SAMPLER_MIPMAP_MODE_NEAREST;
	case MipmapMode::Linear:
		return VK_SAMPLER_MIPMAP_MODE_LINEAR;
	}
	return VK_SAMPLER_MIPMAP_MODE_LINEAR;
}

AQUILA_FORCE_INLINE VkSamplerAddressMode to_vk_address_mode(AddressMode mode) {
	switch (mode) {
	case AddressMode::Repeat:
		return VK_SAMPLER_ADDRESS_MODE_REPEAT;
	case AddressMode::MirroredRepeat:
		return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
	case AddressMode::ClampToEdge:
		return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	case AddressMode::ClampToBorder:
		return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	}
	return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
}

AQUILA_FORCE_INLINE VkBorderColor to_vk_border_color(BorderColor color) {
	switch (color) {
	case BorderColor::TransparentBlack:
		return VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
	case BorderColor::OpaqueBlack:
		return VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	case BorderColor::OpaqueWhite:
		return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	}
	return VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
}

AQUILA_FORCE_INLINE VkSamplerCreateInfo to_vk_sampler_create_info(const SamplerDesc &desc, float max_anisotropy_limit) {
	VkSamplerCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	info.magFilter = to_vk_filter(desc.mag_filter);
	info.minFilter = to_vk_filter(desc.min_filter);
	info.mipmapMode = to_vk_mipmap_mode(desc.mipmap_mode);
	info.addressModeU = to_vk_address_mode(desc.address_u);
	info.addressModeV = to_vk_address_mode(desc.address_v);
	info.addressModeW = to_vk_address_mode(desc.address_w);
	info.borderColor = to_vk_border_color(desc.border_color);
	info.mipLodBias = desc.mip_lod_bias;
	info.minLod = desc.min_lod;
	info.maxLod = desc.max_lod;
	info.unnormalizedCoordinates = VK_FALSE;
	info.anisotropyEnable = desc.anisotropy ? VK_TRUE : VK_FALSE;
	info.maxAnisotropy = desc.anisotropy ? max_anisotropy_limit : 1.0F;
	info.compareEnable = desc.compare_enable ? VK_TRUE : VK_FALSE;
	info.compareOp = to_vk_compare_op(desc.compare_op);
	return info;
}

} // namespace Aquila::RHI
