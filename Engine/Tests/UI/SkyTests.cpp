#include <doctest.h>

#include "GpuTestSupport.h"

#include "Aquila/Foundation/FrameScheduler.h"
#include "Aquila/Foundation/JobSystem.h"
#include "Aquila/GFX/GfxBuffer.h"
#include "Aquila/GFX/GfxCommandList.h"
#include "Aquila/Graphics/RenderGraph/RGGraph.h"
#include "Aquila/Graphics/RenderGraph/RGPassBuilder.h"
#include "Aquila/Graphics/Shader/ShaderHotReload.h"
#include "Aquila/RHI/Vulkan/VulkanShaderCompiler.h"
#include "Aquila/Rendering/Environment/EnvironmentImage.h"
#include "Aquila/Rendering/Environment/HosekWilkieSky.h"
#include "Aquila/Rendering/Environment/SphericalHarmonics.h"
#include "Aquila/Rendering/FrameContext.h"
#include "Aquila/Rendering/SceneFrameData.h"
#include "Aquila/Rendering/Systems/SkySystem.h"
#include "Aquila/Scene/Entity.h"
#include "Aquila/Scene/EntityManager.h"
#include "Aquila/Scene/Scene.h"
#include "Aquila/Scene/Components/CameraComponent.h"
#include "Aquila/Scene/Components/SkyLightComponent.h"
#include "Aquila/Scene/Components/TransformComponent.h"

#include <glm/gtc/packing.hpp>

using namespace Aquila;
using Rendering::EnvironmentImage;
using SceneManagement::Components::SHCoefficients;
using SceneManagement::Components::SkyLightComponent;
using SceneManagement::Components::SkySource;

namespace {

using Radiance = std::function<Vec3(F32 u, F32 v)>;

std::vector<F32> equirect(Uint32 width, Uint32 height, const Radiance &radiance) {
	std::vector<F32> rgba(static_cast<Usize>(width) * height * 4);
	for (Uint32 y = 0; y < height; ++y) {
		for (Uint32 x = 0; x < width; ++x) {
			const Vec3 value = radiance((static_cast<F32>(x) + 0.5F) / static_cast<F32>(width),
										(static_cast<F32>(y) + 0.5F) / static_cast<F32>(height));
			F32 *texel = &rgba[((static_cast<Usize>(y) * width) + x) * 4];
			texel[0] = value.r;
			texel[1] = value.g;
			texel[2] = value.b;
			texel[3] = 1.F;
		}
	}
	return rgba;
}

Vec3 evaluate_sh(const SHCoefficients &sh, Vec3 normal) {
	const std::array<F32, 9> basis = Rendering::SH::basis(Math::normalize(normal));
	Vec3 result(0.F);
	for (Usize i = 0; i < basis.size(); ++i) {
		result += sh.coeffs[i] * basis[i];
	}
	return result;
}

SHCoefficients sh_of(Uint32 width, Uint32 height, const Radiance &radiance) {
	const std::vector<F32> image = equirect(width, height, radiance);
	return Rendering::bake_equirect_sh(image.data(), width, height);
}

bool is_near(Vec3 a, Vec3 b, F32 tolerance) {
	return Math::length(a - b) <= tolerance * Math::max(Math::length(b), 1e-3F);
}

Vec4 half_texel(const std::vector<Uint8> &level, Usize index) {
	std::array<Uint16, 4> halves{};
	std::memcpy(halves.data(), &level[index * sizeof(halves)], sizeof(halves));
	return { glm::unpackHalf1x16(halves[0]), glm::unpackHalf1x16(halves[1]), glm::unpackHalf1x16(halves[2]),
			 glm::unpackHalf1x16(halves[3]) };
}

bool in_patch(F32 u, F32 v, F32 centre_u) {
	return Math::abs(u - centre_u) < 0.1F && Math::abs(v - 0.5F) < 0.1F;
}

}

TEST_SUITE("Environment image") {
	TEST_CASE("A sky of one radiance lights every normal with that radiance") {
		const Vec3 radiance(2.F, 1.F, 0.5F);
		const SHCoefficients sh = sh_of(64, 32, [&](F32, F32) { return radiance; });
		for (const Vec3 normal : { Vec3(1, 0, 0), Vec3(-1, 0, 0), Vec3(0, 1, 0), Vec3(0, -1, 0), Vec3(0, 0, 1),
								   Vec3(0, 0, -1), Vec3(1, 1, 1) }) {
			CHECK(is_near(evaluate_sh(sh, normal), radiance, 0.01F));
		}
	}

	TEST_CASE("The top of the image is up") {
		const SHCoefficients sh = sh_of(64, 32, [](F32, F32 v) { return Vec3(v < 0.5F ? 1.F : 0.F); });
		CHECK(evaluate_sh(sh, Vec3(0, 1, 0)).g > 0.9F);
		CHECK(evaluate_sh(sh, Vec3(0, -1, 0)).g < 0.1F);
		CHECK(evaluate_sh(sh, Vec3(1, 0, 0)).g == doctest::Approx(0.5F).epsilon(0.05F));
	}

	TEST_CASE("The middle of the image looks down +Z, a quarter to the right of it down +X") {
		const SHCoefficients front =
			sh_of(128, 64, [](F32 u, F32 v) { return Vec3(in_patch(u, v, 0.5F) ? 1.F : 0.F); });
		CHECK(evaluate_sh(front, Vec3(0, 0, 1)).r > 2.F * evaluate_sh(front, Vec3(0, 0, -1)).r);
		CHECK(evaluate_sh(front, Vec3(0, 0, 1)).r > evaluate_sh(front, Vec3(1, 0, 0)).r);
		CHECK(evaluate_sh(front, Vec3(1, 0, 0)).r == doctest::Approx(evaluate_sh(front, Vec3(-1, 0, 0)).r));

		const SHCoefficients right =
			sh_of(128, 64, [](F32 u, F32 v) { return Vec3(in_patch(u, v, 0.75F) ? 1.F : 0.F); });
		CHECK(evaluate_sh(right, Vec3(1, 0, 0)).r > 2.F * evaluate_sh(right, Vec3(-1, 0, 0)).r);
		CHECK(evaluate_sh(right, Vec3(1, 0, 0)).r > evaluate_sh(right, Vec3(0, 0, 1)).r);
	}

	TEST_CASE("An image keeps a full chain of half float levels") {
		const std::vector<F32> pixels = equirect(64, 32, [](F32, F32) { return Vec3(3.F); });
		const EnvironmentImage image = EnvironmentImage::build(pixels.data(), 64, 32);
		REQUIRE(image.is_valid());
		CHECK(image.mips.width == 64);
		CHECK(image.mips.height == 32);
		REQUIRE(image.mips.levels.size() == 7);
		for (Usize mip = 0; mip < image.mips.levels.size(); ++mip) {
			const Usize width = std::max<Usize>(1, 64 >> mip);
			const Usize height = std::max<Usize>(1, 32 >> mip);
			CHECK(image.mips.levels[mip].size() == width * height * 8);
			CHECK(half_texel(image.mips.levels[mip], 0).r == doctest::Approx(3.F));
			CHECK(half_texel(image.mips.levels[mip], 0).a == doctest::Approx(1.F));
		}
	}

	TEST_CASE(
		"A sun too bright for half floats is clamped in the fine levels and keeps its energy in the coarse ones") {
		const std::vector<F32> pixels =
			equirect(4, 2, [](F32 u, F32 v) { return Vec3(u < 0.25F && v < 0.5F ? 100000.F : 0.F); });
		const EnvironmentImage image = EnvironmentImage::build(pixels.data(), 4, 2);
		REQUIRE(image.mips.levels.size() == 3);
		CHECK(half_texel(image.mips.levels[0], 0).r == doctest::Approx(65504.F));
		CHECK(half_texel(image.mips.levels[1], 0).r == doctest::Approx(25000.F).epsilon(0.001F));
		CHECK(half_texel(image.mips.levels[2], 0).r == doctest::Approx(12500.F).epsilon(0.001F));
	}

	TEST_CASE("Images wider than the limit lose their top levels") {
		constexpr Uint32 K_WIDTH = EnvironmentImage::K_MAX_WIDTH * 2;
		const std::vector<F32> pixels = equirect(K_WIDTH, 1, [](F32, F32) { return Vec3(1.F); });
		const EnvironmentImage image = EnvironmentImage::build(pixels.data(), K_WIDTH, 1);
		CHECK(image.mips.width == EnvironmentImage::K_MAX_WIDTH);
		CHECK(image.mips.height == 1);
		CHECK(image.mips.levels.size() == 13);
	}

	TEST_CASE("Negative and NaN texels come in black") {
		const std::vector<F32> pixels = equirect(
			2, 1, [](F32 u, F32) { return u < 0.5F ? Vec3(-4.F) : Vec3(std::numeric_limits<F32>::quiet_NaN()); });
		const EnvironmentImage image = EnvironmentImage::build(pixels.data(), 2, 1);
		CHECK(half_texel(image.mips.levels[0], 0).r == 0.F);
		CHECK(half_texel(image.mips.levels[0], 1).r == 0.F);
		CHECK(evaluate_sh(image.irradiance, Vec3(0, 1, 0)).r == 0.F);
	}
}

namespace {

void write_hdr(const std::filesystem::path &path, Uint32 width, Uint32 height, const Radiance &radiance) {
	std::ofstream file(path, std::ios::binary);
	file << "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y " << height << " +X " << width << "\n";
	for (Uint32 y = 0; y < height; ++y) {
		for (Uint32 x = 0; x < width; ++x) {
			const Vec3 value = radiance((static_cast<F32>(x) + 0.5F) / static_cast<F32>(width),
										(static_cast<F32>(y) + 0.5F) / static_cast<F32>(height));
			const F32 brightest = std::max({ value.r, value.g, value.b });
			std::array<Uint8, 4> rgbe{};
			if (brightest > 1e-32F) {
				int exponent = 0;
				const F32 scale = std::frexp(brightest, &exponent) * 256.F / brightest;
				rgbe = { static_cast<Uint8>(value.r * scale), static_cast<Uint8>(value.g * scale),
						 static_cast<Uint8>(value.b * scale), static_cast<Uint8>(exponent + 128) };
			}
			file.write(reinterpret_cast<const char *>(rgbe.data()), rgbe.size());
		}
	}
}

struct SkyFixture {
	static constexpr std::string_view K_MOUNT = "/sky-tests";

	GFX::GfxContext &ctx = *test_gpu().ctx;
	std::filesystem::path dir;
	SceneManagement::Scene scene{ "Sky" };
	SkyLightComponent *sky = nullptr;
	Unique<Rendering::SkySystem> system;
	bool owns_jobs = false;
	bool owns_scheduler = false;

	SkyFixture() {
		dir = std::filesystem::temp_directory_path() /
			("aquila-sky-tests-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
		std::filesystem::create_directories(dir);
		test_vfs().mount(std::string(K_MOUNT), std::make_shared<Platform::Filesystem::NativeFileSystem>(dir.string()));

		RHI::VulkanShaderCompiler::initialize();
		Graphics::Shader::ShaderHotReload::init();
		owns_jobs = !Foundation::JobSystem::is_initialized();
		if (owns_jobs) {
			Foundation::JobSystem::init(1);
		}
		owns_scheduler = !Foundation::FrameScheduler::is_initialized();
		if (owns_scheduler) {
			Foundation::FrameScheduler::init();
		}
		Rendering::SceneFrameData::init(ctx, 64, 64);
		system = std::make_unique<Rendering::SkySystem>();
		system->on_init(ctx);

		sky = &scene.get_entity_manager()->create_entity("Sky").add_component<SkyLightComponent>();
	}

	~SkyFixture() {
		ctx.wait_idle();
		system.reset();
		Rendering::SceneFrameData::shutdown();
		if (owns_scheduler) {
			Foundation::FrameScheduler::shutdown();
		}
		if (owns_jobs) {
			Foundation::JobSystem::shutdown();
		}
		Graphics::Shader::ShaderHotReload::shutdown();
		test_vfs().unmount(std::string(K_MOUNT));
		std::error_code error;
		std::filesystem::remove_all(dir, error);
	}

	SkyFixture(const SkyFixture &) = delete;
	SkyFixture &operator=(const SkyFixture &) = delete;

	std::string image(const std::string &name, const Radiance &radiance, Uint32 width = 64, Uint32 height = 32) {
		write_hdr(dir / name, width, height, radiance);
		return std::string(K_MOUNT) + "/" + name;
	}

	void frame() {
		Graphics::RG::RenderGraph graph;
		Rendering::FrameContext frame;
		frame.scene = &scene;
		system->prepare_passes(graph, frame);
		graph.add_pass(
			"KeepGraphBusy", [](Graphics::RG::RGPassBuilder &builder) { builder.mark_as_side_effect(); },
			[](GFX::GfxCommandList &, Graphics::RG::RGRegistry &) {});
		graph.compile(ctx);
		ctx.execute_immediate(RHI::CommandListType::Graphics, [&](GFX::GfxCommandList &cmd) { graph.execute(cmd); });
	}

	void frame_after_loading() {
		frame();
		Foundation::JobSystem::get()->wait_for_all();
		frame();
	}

	std::vector<Vec4> read(GFX::GfxTexture &cube, Uint32 face, Uint32 mip) {
		const Uint32 size = std::max(1U, cube.get_width() >> mip);
		Ref<GFX::GfxBuffer> buffer = ctx.create_buffer({
			.size = static_cast<Uint64>(size) * size * 8,
			.usage = RHI::BufferUsage::TransferDst,
			.domain = RHI::MemoryDomain::GpuToCpu,
			.debug_name = "SkyTestReadback",
		});
		ctx.execute_immediate(RHI::CommandListType::Graphics, [&](GFX::GfxCommandList &cmd) {
			cmd.transition_texture(cube, RHI::ResourceState::ShaderRead, RHI::ResourceState::TransferSrc);
			cmd.copy_texture_to_buffer(cube, *buffer, size, size, face, mip);
			cmd.transition_texture(cube, RHI::ResourceState::TransferSrc, RHI::ResourceState::ShaderRead);
		});
		const auto *mapped = static_cast<const Uint8 *>(buffer->map());
		std::vector<Uint8> bytes(mapped, mapped + (static_cast<Usize>(size) * size * 8));
		buffer->unmap();
		std::vector<Vec4> texels(static_cast<Usize>(size) * size);
		for (Usize i = 0; i < texels.size(); ++i) {
			texels[i] = half_texel(bytes, i);
		}
		return texels;
	}

	Vec3 centre(GFX::GfxTexture &cube, Uint32 face, Uint32 mip = 0) {
		const Uint32 size = std::max(1U, cube.get_width() >> mip);
		return Vec3(read(cube, face, mip)[(static_cast<Usize>(size / 2) * size) + (size / 2)]);
	}

	std::vector<Vec4> draw(Uint32 size) {
		SceneManagement::Entity camera = scene.get_entity_manager()->create_entity("Camera");
		const Rendering::RenderView view = Rendering::render_view_from_entity(
			camera.add_component<SceneManagement::Components::CameraComponent>(),
			camera.get_component<SceneManagement::Components::TransformComponent>(), 1.F);
		Rendering::SceneFrameData::get()->update(scene, 0.F, 0, view, 1.F, false);

		Ref<GFX::GfxTexture> color = ctx.create_texture({
			.width = size,
			.height = size,
			.format = RHI::TextureFormat::RGBA16F,
			.usage = RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::Sampled | RHI::TextureUsage::TransferSrc,
			.debug_name = "SkyTestColor",
		});
		Ref<GFX::GfxTexture> depth = ctx.create_texture({
			.width = size,
			.height = size,
			.format = RHI::TextureFormat::Depth32,
			.usage = RHI::TextureUsage::DepthAttachment,
			.debug_name = "SkyTestDepth",
		});

		Graphics::RG::RenderGraph graph;
		Rendering::FrameContext frame;
		frame.scene = &scene;
		frame.h_scene_color = graph.import_texture(color.get(), "SceneColor");
		frame.h_depth = graph.import_texture(depth.get(), "Depth");
		graph.add_pass(
			"Clear",
			[&frame](Graphics::RG::RGPassBuilder &builder) {
				frame.h_scene_color = builder.set_color_attachment(0, frame.h_scene_color);
				frame.h_depth = builder.set_depth_attachment(frame.h_depth);
			},
			[](GFX::GfxCommandList &, Graphics::RG::RGRegistry &) {});
		system->prepare_passes(graph, frame);
		system->add_passes(graph, frame);
		graph.add_pass(
			"Readable",
			[&frame](Graphics::RG::RGPassBuilder &builder) {
				builder.read_texture(frame.h_scene_color);
				builder.mark_as_side_effect();
			},
			[](GFX::GfxCommandList &, Graphics::RG::RGRegistry &) {});
		graph.compile(ctx);
		ctx.execute_immediate(RHI::CommandListType::Graphics, [&](GFX::GfxCommandList &cmd) { graph.execute(cmd); });
		return read(*color, 0, 0);
	}

	GFX::GfxTexture &source() const { return system->get_source_cube(); }
	static GFX::GfxTexture &reflection() { return Rendering::SceneFrameData::get()->get_reflection_cube(); }
};

}

TEST_SUITE("Sky") {
	TEST_CASE("An image sky of one radiance fills the source and reflection cubes with it") {
		SkyFixture fixture;
		const Vec3 radiance(2.F, 1.F, 0.5F);
		fixture.sky->set_source(SkySource::HdrImage);
		fixture.sky->set_hdr_path(fixture.image("flat.hdr", [&](F32, F32) { return radiance; }));

		fixture.frame();
		CHECK(fixture.system->is_loading_image());
		Foundation::JobSystem::get()->wait_for_all();
		fixture.frame();
		CHECK_FALSE(fixture.system->is_loading_image());
		CHECK_FALSE(fixture.sky->is_dirty());

		for (Uint32 face = 0; face < 6; ++face) {
			for (const Uint32 mip : { 0U, 4U, Rendering::SkySystem::K_SOURCE_MIPS - 1 }) {
				for (const Vec4 &texel : fixture.read(fixture.source(), face, mip)) {
					REQUIRE(is_near(Vec3(texel), radiance, 0.01F));
				}
			}
			for (const Uint32 mip : { 0U, 3U, Rendering::SceneFrameData::K_REFLECTION_MIPS - 1 }) {
				for (const Vec4 &texel : fixture.read(SkyFixture::reflection(), face, mip)) {
					REQUIRE(is_near(Vec3(texel), radiance, 0.02F));
				}
			}
		}
		CHECK(is_near(evaluate_sh(fixture.sky->get_irradiance(), Vec3(0, 1, 0)), radiance, 0.01F));
	}

	TEST_CASE("The source cube faces look where the image says") {
		SkyFixture fixture;
		fixture.sky->set_source(SkySource::HdrImage);
		fixture.sky->set_hdr_path(fixture.image("marked.hdr", [](F32 u, F32 v) {
			if (in_patch(u, v, 0.5F)) {
				return Vec3(50.F);
			}
			if (in_patch(u, v, 0.75F)) {
				return Vec3(20.F);
			}
			return Vec3(v < 0.5F ? 4.F : 0.F);
		}));
		fixture.frame_after_loading();

		CHECK(fixture.centre(fixture.source(), 0).r == doctest::Approx(20.F).epsilon(0.01F));
		CHECK(fixture.centre(fixture.source(), 1).r < 5.F);
		CHECK(fixture.centre(fixture.source(), 2).r == doctest::Approx(4.F).epsilon(0.01F));
		CHECK(fixture.centre(fixture.source(), 3).r == doctest::Approx(0.F));
		CHECK(fixture.centre(fixture.source(), 4).r == doctest::Approx(50.F).epsilon(0.01F));
		CHECK(fixture.centre(fixture.source(), 5).r < 5.F);
		CHECK(fixture.centre(SkyFixture::reflection(), 4).r == doctest::Approx(50.F).epsilon(0.02F));
	}

	TEST_CASE("A small bright sun blurs into rough reflections without speckles") {
		SkyFixture fixture;
		fixture.sky->set_source(SkySource::HdrImage);
		fixture.sky->set_hdr_path(fixture.image(
			"sun.hdr",
			[](F32 u, F32 v) {
				return Math::abs(u - 0.5F) < 0.002F && Math::abs(v - 0.3F) < 0.004F ? Vec3(60000.F) : Vec3(1.F);
			},
			1024, 512));
		fixture.frame_after_loading();

		for (Uint32 mip = 1; mip < Rendering::SceneFrameData::K_REFLECTION_MIPS - 1; ++mip) {
			const Uint32 size = Rendering::SceneFrameData::K_REFLECTION_SIZE >> mip;
			const std::vector<Vec4> texels = fixture.read(SkyFixture::reflection(), 4, mip);
			F32 worst = 0.F;
			for (Uint32 y = 1; y + 1 < size; ++y) {
				for (Uint32 x = 1; x + 1 < size; ++x) {
					const auto at = [&](Uint32 tx, Uint32 ty) {
						return texels[(static_cast<Usize>(ty) * size) + tx].r;
					};
					const F32 neighbours = 0.25F * (at(x - 1, y) + at(x + 1, y) + at(x, y - 1) + at(x, y + 1));
					worst = Math::max(worst, Math::abs(at(x, y) - neighbours) / neighbours);
				}
			}
			CAPTURE(mip);
			CHECK(worst < 0.5F);
		}
	}

	TEST_CASE("The image sky is drawn the right way up and round, times its intensity") {
		SkyFixture fixture;
		fixture.sky->set_source(SkySource::HdrImage);
		fixture.sky->set_image_intensity(2.F);
		fixture.sky->set_hdr_path(fixture.image(
			"front.hdr",
			[](F32 u, F32 v) {
				if (in_patch(u, v, 0.5F)) {
					return Vec3(u < 0.5F ? 30.F : 50.F);
				}
				return Vec3(v < 0.5F ? 4.F : 0.F);
			},
			256, 128));
		fixture.frame_after_loading();

		constexpr Uint32 K_SIZE = 64;
		const std::vector<Vec4> pixels = fixture.draw(K_SIZE);
		const auto at = [&](Uint32 x, Uint32 y) { return pixels[(static_cast<Usize>(y) * K_SIZE) + x].r; };
		CHECK(at(16, 32) == doctest::Approx(60.F).epsilon(0.01F));
		CHECK(at(48, 32) == doctest::Approx(100.F).epsilon(0.01F));
		CHECK(at(32, 1) == doctest::Approx(8.F).epsilon(0.01F));
		CHECK(at(32, K_SIZE - 2) == doctest::Approx(0.F));
	}

	TEST_CASE("Only the image asked for last shows up") {
		SkyFixture fixture;
		fixture.sky->set_source(SkySource::HdrImage);
		fixture.sky->set_hdr_path(fixture.image("first.hdr", [](F32, F32) { return Vec3(1.F); }));
		fixture.frame();
		fixture.sky->set_hdr_path(fixture.image("second.hdr", [](F32, F32) { return Vec3(8.F); }));
		fixture.frame_after_loading();

		CHECK(fixture.centre(fixture.source(), 2).r == doctest::Approx(8.F).epsilon(0.01F));
	}

	TEST_CASE("A file that isn't an image gives no sky and stops loading") {
		SkyFixture fixture;
		std::ofstream(fixture.dir / "broken.hdr") << "not an image";
		fixture.sky->set_source(SkySource::HdrImage);
		fixture.sky->set_hdr_path(std::string(SkyFixture::K_MOUNT) + "/broken.hdr");
		fixture.frame_after_loading();

		CHECK_FALSE(fixture.system->is_loading_image());
		CHECK(fixture.sky->is_dirty());
	}

	TEST_CASE("The procedural sky goes through the same source cube") {
		SkyFixture fixture;
		fixture.frame();

		const Rendering::GpuSkyData data = Rendering::HosekWilkieSky::build(
			Math::radians(fixture.sky->get_sun_elevation()), fixture.sky->get_turbidity(),
			fixture.sky->get_ground_albedo(), fixture.sky->get_sun_direction());
		const Vec3 zenith = Rendering::HosekWilkieSky::evaluate(data, Vec3(0, 1, 0));
		CHECK(is_near(fixture.centre(fixture.source(), 2), zenith, 0.02F));
		CHECK(is_near(fixture.centre(SkyFixture::reflection(), 2), zenith, 0.03F));
		CHECK_FALSE(fixture.sky->is_dirty());
	}
}
