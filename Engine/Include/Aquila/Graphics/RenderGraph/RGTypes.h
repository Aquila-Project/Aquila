#pragma once
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/RHI/Backend/RHITypes.h"

namespace Aquila::Graphics::RG {

template <typename Tag> struct RGHandle {
	static constexpr Uint32 INVALID = UINT32_MAX;
	Uint32 id = INVALID;

	[[nodiscard]] bool is_valid() const { return id != INVALID; }
	bool operator==(const RGHandle &) const = default;
};

struct RGTextureTag {};
struct RGBufferTag {};

using RGTextureHandle = RGHandle<RGTextureTag>;
using RGBufferHandle = RGHandle<RGBufferTag>;

using ResourceState = RHI::ResourceState;

struct RGTextureDesc {
	Uint32 width = 1;
	Uint32 height = 1;
	Uint32 mip_levels = 1;
	Uint32 array_layers = 1;
	RHI::TextureFormat format = RHI::TextureFormat::RGBA8;
	RHI::TextureUsage usage = RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::Sampled;
	RHI::SampleCount samples = RHI::SampleCount::X1;
	std::string_view debug_name;
};

struct RGBufferDesc {
	Uint64 size = 0;
	RHI::BufferUsage usage = RHI::BufferUsage::None;
	RHI::MemoryDomain domain = RHI::MemoryDomain::GpuOnly;
	std::string_view debug_name;
};

using AttachmentLoadOp = RHI::AttachmentLoadOp;
using AttachmentStoreOp = RHI::AttachmentStoreOp;

struct ClearColor {
	Vec4 color;
};

struct ClearDepth {
	float depth = 1.F;
	Uint8 stencil = 0;
};

} // namespace Aquila::Graphics::RG
