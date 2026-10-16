#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <GLFW/glfw3.h>

#include "Aquila/GFX/GfxContext.h"

using namespace Aquila;

namespace {

struct GfxFixture {
	GLFWwindow *window = nullptr;
	Unique<GFX::GfxContext> ctx;

	GfxFixture() {
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
		window = glfwCreateWindow(800, 600, "RHITests", nullptr, nullptr);
		REQUIRE(window != nullptr);
		ctx = GFX::GfxContext::create(*window);
		REQUIRE(ctx != nullptr);
	}

	~GfxFixture() {
		ctx->wait_idle();
		ctx.reset();
		glfwDestroyWindow(window);
		glfwTerminate();
	}
};

static GfxFixture *g_Fixture = nullptr;

GfxFixture &Fixture() {
	if (!g_Fixture) {
		g_Fixture = new GfxFixture();
	}
	return *g_Fixture;
}

GFX::GfxContext &Ctx() {
	return *Fixture().ctx;
}

} // namespace

// [Device]

TEST_SUITE("Device") {
	TEST_CASE("Context creates successfully") {
		CHECK(Fixture().ctx != nullptr);
	}

	TEST_CASE("Underlying device is accessible and valid") {
		RHI::IRHIDevice *dev = &Ctx().get_device();
		CHECK(dev != nullptr);
	}

	TEST_CASE("WaitIdle does not crash") {
		CHECK_NOTHROW(Ctx().wait_idle());
	}
}

// [Buffer]

TEST_SUITE("Buffer") {
	TEST_CASE("GpuOnly vertex buffer creates successfully") {
		RHI::BufferDesc desc{};
		desc.size = sizeof(float) * 12;
		desc.usage = RHI::BufferUsage::VertexBuffer | RHI::BufferUsage::TransferDst;
		desc.domain = RHI::MemoryDomain::GpuOnly;
		desc.debug_name = "Test_VertexBuf";

		auto buf = Ctx().create_buffer(desc);
		CHECK(buf != nullptr);
	}

	TEST_CASE("CpuToGpu uniform buffer creates successfully") {
		RHI::BufferDesc desc{};
		desc.size = 256;
		desc.usage = RHI::BufferUsage::UniformBuffer;
		desc.domain = RHI::MemoryDomain::CpuToGpu;
		desc.debug_name = "Test_UniformBuf";

		auto buf = Ctx().create_buffer(desc);
		CHECK(buf != nullptr);
	}

	TEST_CASE("CpuOnly staging buffer creates successfully") {
		RHI::BufferDesc desc{};
		desc.size = 1024;
		desc.usage = RHI::BufferUsage::TransferSrc;
		desc.domain = RHI::MemoryDomain::CpuOnly;
		desc.debug_name = "Test_StagingBuf";

		auto buf = Ctx().create_buffer(desc);
		CHECK(buf != nullptr);
	}

	TEST_CASE("GpuToCpu readback buffer creates successfully") {
		RHI::BufferDesc desc{};
		desc.size = 512;
		desc.usage = RHI::BufferUsage::TransferDst;
		desc.domain = RHI::MemoryDomain::GpuToCpu;
		desc.debug_name = "Test_ReadbackBuf";

		auto buf = Ctx().create_buffer(desc);
		CHECK(buf != nullptr);
	}

	TEST_CASE("Multiple buffers have distinct handles") {
		RHI::BufferDesc desc{};
		desc.size = 64;
		desc.usage = RHI::BufferUsage::UniformBuffer;
		desc.domain = RHI::MemoryDomain::CpuToGpu;

		desc.debug_name = "BufA";
		auto a = Ctx().create_buffer(desc);
		desc.debug_name = "BufB";
		auto b = Ctx().create_buffer(desc);

		CHECK(a.get() != b.get());
	}

	TEST_CASE("Storage buffer creates successfully") {
		RHI::BufferDesc desc{};
		desc.size = 2048;
		desc.usage = RHI::BufferUsage::StorageBuffer | RHI::BufferUsage::TransferDst;
		desc.domain = RHI::MemoryDomain::GpuOnly;
		desc.debug_name = "Test_StorageBuf";

		auto buf = Ctx().create_buffer(desc);
		CHECK(buf != nullptr);
	}
}

// [Texture]

TEST_SUITE("Texture") {
	TEST_CASE("RGBA8 sampled texture creates successfully") {
		RHI::TextureDesc desc{};
		desc.width = 256;
		desc.height = 256;
		desc.format = RHI::TextureFormat::RGBA8;
		desc.usage = RHI::TextureUsage::Sampled | RHI::TextureUsage::TransferDst;
		desc.debug_name = "Test_RGBA8";

		auto tex = Ctx().create_texture(desc);
		CHECK(tex != nullptr);
	}

	TEST_CASE("RGBA16F storage texture creates successfully") {
		RHI::TextureDesc desc{};
		desc.width = 512;
		desc.height = 512;
		desc.format = RHI::TextureFormat::RGBA16F;
		desc.usage = RHI::TextureUsage::Storage | RHI::TextureUsage::Sampled;
		desc.debug_name = "Test_RGBA16F_Storage";

		auto tex = Ctx().create_texture(desc);
		CHECK(tex != nullptr);
	}

	TEST_CASE("Depth32 attachment texture creates successfully") {
		RHI::TextureDesc desc{};
		desc.width = 1280;
		desc.height = 720;
		desc.format = RHI::TextureFormat::Depth32;
		desc.usage = RHI::TextureUsage::DepthAttachment | RHI::TextureUsage::Sampled;
		desc.debug_name = "Test_Depth";

		auto tex = Ctx().create_texture(desc);
		CHECK(tex != nullptr);
	}

	TEST_CASE("Color attachment texture creates successfully") {
		RHI::TextureDesc desc{};
		desc.width = 1280;
		desc.height = 720;
		desc.format = RHI::TextureFormat::RGBA8;
		desc.usage = RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::Sampled;
		desc.debug_name = "Test_ColorAttachment";

		auto tex = Ctx().create_texture(desc);
		CHECK(tex != nullptr);
	}

	TEST_CASE("Texture reports correct dimensions") {
		RHI::TextureDesc desc{};
		desc.width = 128;
		desc.height = 64;
		desc.format = RHI::TextureFormat::RGBA8;
		desc.usage = RHI::TextureUsage::Sampled | RHI::TextureUsage::TransferDst;
		desc.debug_name = "Test_DimCheck";

		auto tex = Ctx().create_texture(desc);
		REQUIRE(tex != nullptr);
		CHECK(tex->get_width() == 128u);
		CHECK(tex->get_height() == 64u);
	}

	TEST_CASE("Texture reports correct format") {
		RHI::TextureDesc desc{};
		desc.width = 64;
		desc.height = 64;
		desc.format = RHI::TextureFormat::RGBA32F;
		desc.usage = RHI::TextureUsage::Sampled | RHI::TextureUsage::TransferDst;
		desc.debug_name = "Test_FormatCheck";

		auto tex = Ctx().create_texture(desc);
		REQUIRE(tex != nullptr);
		CHECK(tex->get_format() == RHI::TextureFormat::RGBA32F);
	}

	TEST_CASE("Mipped texture creates successfully") {
		RHI::TextureDesc desc{};
		desc.width = 512;
		desc.height = 512;
		desc.mip_levels = 4;
		desc.format = RHI::TextureFormat::RGBA8;
		desc.usage = RHI::TextureUsage::Sampled | RHI::TextureUsage::TransferDst | RHI::TextureUsage::TransferSrc;
		desc.debug_name = "Test_Mipped";

		auto tex = Ctx().create_texture(desc);
		CHECK(tex != nullptr);
	}
}

// [Swapchain]

TEST_SUITE("Swapchain") {
	TEST_CASE("Swapchain creates successfully") {
		RHI::SwapchainDesc desc{};
		desc.width = 800;
		desc.height = 600;
		desc.format = RHI::TextureFormat::BGRA8;
		desc.image_count = 2;
		desc.vsync = true;

		auto sc = Ctx().create_swapchain(desc);
		CHECK(sc != nullptr);
	}

	TEST_CASE("Swapchain reports at least one image") {
		RHI::SwapchainDesc desc{ .width = 800, .height = 600, .format = RHI::TextureFormat::BGRA8 };
		auto sc = Ctx().create_swapchain(desc);
		REQUIRE(sc != nullptr);
		CHECK(sc->get_image_count() >= 1u);
	}

	TEST_CASE("Swapchain reports non-zero dimensions") {
		RHI::SwapchainDesc desc{ .width = 800, .height = 600, .format = RHI::TextureFormat::BGRA8 };
		auto sc = Ctx().create_swapchain(desc);
		REQUIRE(sc != nullptr);
		CHECK(sc->get_width() > 0u);
		CHECK(sc->get_height() > 0u);
	}

	TEST_CASE("Swapchain format is not None") {
		RHI::SwapchainDesc desc{ .width = 800, .height = 600, .format = RHI::TextureFormat::BGRA8 };
		auto sc = Ctx().create_swapchain(desc);
		REQUIRE(sc != nullptr);
		CHECK(sc->get_format() != RHI::TextureFormat::None);
	}
}

// [CommandList]

TEST_SUITE("CommandList") {
	TEST_CASE("Graphics command list creates successfully") {
		auto cmd = Ctx().create_command_list(RHI::CommandListType::Graphics, "Test_Graphics");
		CHECK(cmd != nullptr);
	}

	TEST_CASE("Compute command list creates successfully") {
		auto cmd = Ctx().create_command_list(RHI::CommandListType::Compute, "Test_Compute");
		CHECK(cmd != nullptr);
	}

	TEST_CASE("Transfer command list creates successfully") {
		auto cmd = Ctx().create_command_list(RHI::CommandListType::Transfer, "Test_Transfer");
		CHECK(cmd != nullptr);
	}

	TEST_CASE("Command list Begin/End cycle does not crash") {
		auto cmd = Ctx().create_command_list(RHI::CommandListType::Graphics, "Test_BeginEnd");
		REQUIRE(cmd != nullptr);
		CHECK_NOTHROW(cmd->begin());
		CHECK_NOTHROW(cmd->end());
	}

	TEST_CASE("SubmitAndWait with empty command list does not crash") {
		auto cmd = Ctx().create_command_list(RHI::CommandListType::Graphics, "Test_SubmitWait");
		REQUIRE(cmd != nullptr);
		cmd->begin();
		cmd->end();
		CHECK_NOTHROW(Ctx().submit_and_wait(*cmd));
	}

	TEST_CASE("ExecuteImmediate does not crash") {
		CHECK_NOTHROW(Ctx().execute_immediate(RHI::CommandListType::Transfer, [](GFX::GfxCommandList &) {}));
	}

	TEST_CASE("Timestamp queries record increasing GPU ticks") {
		Unique<RHI::IRHIQueryPool> pool = Ctx().create_timestamp_pool(4);
		if (pool == nullptr) {
			MESSAGE("Device does not support graphics timestamps; skipping");
			return;
		}
		CHECK(pool->get_count() == 4);
		CHECK(pool->get_nanoseconds_per_tick() > 0.0);

		Ctx().execute_immediate(RHI::CommandListType::Graphics, [&](GFX::GfxCommandList &cmd) {
			cmd.reset_queries(*pool, 0, 2);
			cmd.write_timestamp(*pool, 0);
			cmd.write_timestamp(*pool, 1);
		});

		std::array<Uint64, 2> ticks{};
		REQUIRE(pool->read_timestamps(0, ticks));
		CHECK(ticks[1] >= ticks[0]);
	}

	TEST_CASE("Reading beyond the pool fails cleanly") {
		Unique<RHI::IRHIQueryPool> pool = Ctx().create_timestamp_pool(2);
		if (pool == nullptr) {
			return;
		}
		std::array<Uint64, 3> ticks{};
		CHECK_FALSE(pool->read_timestamps(0, ticks));
	}
}
