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
	}
}
