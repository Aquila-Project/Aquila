#include <doctest.h>

#include "Aquila/RHI/SpirvInspection.h"
#include "Aquila/RHI/Vulkan/VulkanShaderCompiler.h"

using namespace Aquila;

namespace {

std::vector<RHI::VulkanCompiledStage> compile(const std::string &name, const std::string &source) {
	RHI::VulkanShaderCompiler::initialize();
	std::vector<RHI::VulkanCompiledStage> stages;
	std::string error;
	const bool compiled = RHI::VulkanShaderCompiler::compile_source(name, source, stages, error);
	REQUIRE_MESSAGE(compiled, error);
	return stages;
}

bool reads_time(const RHI::VulkanCompiledStage &stage) {
	return RHI::spirv_reads_member(stage.spirv, "FrameData", "time");
}

const char *K_PRELUDE = R"(
import Utility.FrameData;
using namespace Aquila::Shading;
struct VSOut { float4 pos : SV_Position; };
)";

}

TEST_SUITE("SPIR-V inspection") {
	TEST_CASE("a stage that reads the frame time is detected") {
		const auto stages = compile("time_through_helper", std::string(K_PRELUDE) + R"(
[shader("vertex")] VSOut vs_main(float3 p : POSITION) { VSOut o; o.pos = float4(p.x + sin(GetTime()), p.yz, 1.0); return o; }
[shader("fragment")] float4 fs_main() : SV_Target { return float4(frameData.time, 0.0, 0.0, 1.0); }
)");
		REQUIRE(stages.size() == 2);
		CHECK(reads_time(stages[0]));
		CHECK(reads_time(stages[1]));
	}

	TEST_CASE("reading other frame data is not mistaken for time") {
		const auto stages = compile("no_time", std::string(K_PRELUDE) + R"(
[shader("vertex")] VSOut vs_main(float3 p : POSITION) { VSOut o; o.pos = mul(GetMainCamera().viewProjection, float4(p, 1.0)); return o; }
[shader("fragment")] float4 fs_main() : SV_Target { return float4(GetDeltaTime(), float(GetFrameIndex()), 0.0, 1.0); }
)");
		REQUIRE(stages.size() == 2);
		CHECK_FALSE(reads_time(stages[0]));
		CHECK_FALSE(reads_time(stages[1]));
	}

	TEST_CASE("garbage input is rejected") {
		const std::vector<Uint32> words = { 1, 2, 3 };
		CHECK_FALSE(RHI::spirv_reads_member(words, "FrameData", "time"));
		CHECK_FALSE(RHI::spirv_vertex_input_locations(words, "main").has_value());
	}

	TEST_CASE("a vertex stage lists only the inputs it reads") {
		const auto stages = compile("partial_inputs", R"(
struct VSIn {
	[[vk::location(0)]] float3 position : POSITION;
	[[vk::location(1)]] float3 color : COLOR;
	[[vk::location(2)]] float3 normal : NORMAL;
	[[vk::location(3)]] float4 tangent : TANGENT;
	[[vk::location(4)]] float2 uv : TEXCOORD0;
};
struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };
[shader("vertex")] VSOut vs_main(VSIn input, uint id : SV_VertexID) { VSOut o; o.pos = float4(input.position, float(id)); o.uv = input.uv; return o; }
[shader("fragment")] float4 fs_main(VSOut input) : SV_Target { return float4(input.uv, 0.0, 1.0); }
)");
		REQUIRE(stages.size() == 2);
		const auto locations = RHI::spirv_vertex_input_locations(stages[0].spirv, stages[0].entry_point_name);
		REQUIRE(locations.has_value());
		CHECK(*locations == std::vector<Uint32>{ 0, 4 });
	}

	TEST_CASE("a matrix input takes one location per column") {
		const auto stages = compile("matrix_input", R"(
struct VSIn {
	[[vk::location(0)]] float3 position : POSITION;
	[[vk::location(1)]] float4x4 model : MODEL;
};
[shader("vertex")] float4 vs_main(VSIn input) : SV_Position { return mul(input.model, float4(input.position, 1.0)); }
[shader("fragment")] float4 fs_main() : SV_Target { return float4(1.0); }
)");
		REQUIRE(stages.size() == 2);
		const auto locations = RHI::spirv_vertex_input_locations(stages[0].spirv, stages[0].entry_point_name);
		REQUIRE(locations.has_value());
		CHECK(*locations == std::vector<Uint32>{ 0, 1, 2, 3, 4 });
	}

	TEST_CASE("an unknown entry point gives nothing") {
		const auto stages = compile("unknown_entry", std::string(K_PRELUDE) + R"(
[shader("vertex")] VSOut vs_main(float3 p : POSITION) { VSOut o; o.pos = float4(p, 1.0); return o; }
[shader("fragment")] float4 fs_main() : SV_Target { return float4(1.0); }
)");
		REQUIRE(stages.size() == 2);
		CHECK_FALSE(RHI::spirv_vertex_input_locations(stages[0].spirv, "not_there").has_value());
		CHECK_FALSE(RHI::spirv_vertex_input_locations(stages[1].spirv, stages[1].entry_point_name).has_value());
	}
}
