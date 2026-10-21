#ifndef AQUILA_VULKAN_SHADER_COMPILER_H
#define AQUILA_VULKAN_SHADER_COMPILER_H

#include "GraphicsPCH.h"
#include "Aquila/Foundation/EnginePaths.h"
#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"
#include "Aquila/RHI/ShaderPack.h"
#include <slang/slang-com-ptr.h>
#include <slang/slang.h>

namespace Aquila::RHI {

struct VulkanCompiledStage {
	VkShaderStageFlagBits stage{};
	std::vector<Uint32> spirv;
	std::string entry_point_name;
	Slang::ComPtr<slang::IComponentType> linked_component;
	Slang::ComPtr<slang::ISession> session;
};

struct SlangCompileInput {
	std::string name;
	std::string source;
	std::vector<std::string> search_paths;
	std::string entry_module;
};

enum class SlangCompileResult : Uint8 { Ok, NoEntryPoints, Failed };

class VulkanShaderCompiler {
  public:
	static void initialize() {
		if (s_Initialized) {
			return;
		}
		slang::createGlobalSession(s_GlobalSession.writeRef());
		s_Initialized = true;
	}

	static void shutdown() {
		s_GlobalSession = nullptr;
		s_Initialized = false;
	}

	[[nodiscard]] static Slang::ComPtr<slang::IGlobalSession> create_global_session() {
		Slang::ComPtr<slang::IGlobalSession> session;
		slang::createGlobalSession(session.writeRef());
		return session;
	}

	/// One stage per entry point in the file. All or nothing: if one entry point fails,
	/// `out_stages` is empty and `error_log` says why. Shader packs are checked first.
	/// @note Paths with "://" go through the VFS, everything else is read from disk.
	static bool compile_file(const std::string &filepath, std::vector<VulkanCompiledStage> &out_stages,
							 std::string &error_log) {
		AQUILA_ASSERT(s_Initialized, "VulkanShaderCompiler::Initialize() was not called");

		if (const ShaderPackEntry *entry = ShaderPacks::find(shader_key(filepath))) {
			from_pack(*entry, out_stages);
			return true;
		}

		std::string source_code;
		const bool is_virtual_path = filepath.find("://") != std::string::npos;
		if (is_virtual_path) {
			auto *vfs = Platform::Filesystem::VirtualFileSystem::get();
			source_code = vfs->read_text_file(filepath);
			if (source_code.empty()) {
				error_log = "VFS: failed to read shader source from '" + filepath + "'";
				return false;
			}
		} else {
			std::ifstream file(filepath);
			if (!file.is_open()) {
				error_log = "Failed to open shader file '" + filepath + "'";
				return false;
			}
			source_code = { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
		}

		const SlangCompileInput input{
			.name = filepath,
			.source = std::move(source_code),
			.search_paths = {},
			.entry_module = {},
		};
		return compile(*s_GlobalSession, input, out_stages, error_log) == SlangCompileResult::Ok;
	}

	static bool compile_source(const std::string &name, const std::string &source,
							   std::vector<VulkanCompiledStage> &out_stages, std::string &error_log,
							   const std::vector<std::string> &extra_search_paths = {},
							   const std::string &entry_module = {}) {
		AQUILA_ASSERT(s_Initialized, "VulkanShaderCompiler::Initialize() was not called");
		const SlangCompileInput input{
			.name = name,
			.source = source,
			.search_paths = extra_search_paths,
			.entry_module = entry_module,
		};
		return compile(*s_GlobalSession, input, out_stages, error_log) == SlangCompileResult::Ok;
	}

	static SlangCompileResult compile(slang::IGlobalSession &global_session, const SlangCompileInput &input,
									  std::vector<VulkanCompiledStage> &out_stages, std::string &error_log) {
		Slang::ComPtr<slang::ISession> session = create_session(global_session, input.search_paths);
		if (session == nullptr) {
			error_log = "Failed to create Slang session";
			return SlangCompileResult::Failed;
		}

		slang::IModule *slang_module = nullptr;
		{
			Slang::ComPtr<slang::IBlob> diagnostics;
			slang_module = session->loadModuleFromSourceString(input.name.c_str(), input.name.c_str(),
															   input.source.c_str(), diagnostics.writeRef());
			if (slang_module == nullptr) {
				error_log = (diagnostics != nullptr) ? static_cast<const char *>(diagnostics->getBufferPointer())
													 : "Failed to load Slang module '" + input.name + "'";
				return SlangCompileResult::Failed;
			}
		}

		slang::IModule *entry_module = slang_module;
		if (!input.entry_module.empty()) {
			Slang::ComPtr<slang::IBlob> diagnostics;
			entry_module = session->loadModule(input.entry_module.c_str(), diagnostics.writeRef());
			if (entry_module == nullptr) {
				error_log = (diagnostics != nullptr) ? static_cast<const char *>(diagnostics->getBufferPointer())
													 : "Failed to load the engine module '" + input.entry_module + "'";
				return SlangCompileResult::Failed;
			}
		}

		const Int32 entry_point_count = entry_module->getDefinedEntryPointCount();
		if (entry_point_count == 0) {
			error_log = "No entry points found in '" + input.name + "'";
			return SlangCompileResult::NoEntryPoints;
		}

		for (Int32 ep = 0; ep < entry_point_count; ++ep) {
			Slang::ComPtr<slang::IEntryPoint> entry_point;
			if (SLANG_FAILED(entry_module->getDefinedEntryPoint(ep, entry_point.writeRef()))) {
				error_log = "Failed to get entry point " + std::to_string(ep) + " from '" + input.name + "'";
				out_stages.clear();
				return SlangCompileResult::Failed;
			}

			std::vector<slang::IComponentType *> components = { slang_module };
			if (entry_module != slang_module) {
				components.push_back(entry_module);
			}
			components.push_back(entry_point);
			Slang::ComPtr<slang::IComponentType> composed;
			session->createCompositeComponentType(components.data(), static_cast<SlangInt>(components.size()),
												  composed.writeRef());

			slang::ProgramLayout *layout = composed->getLayout();
			slang::EntryPointReflection *ep_ref = layout->getEntryPointByIndex(0);
			VkShaderStageFlagBits vk_stage = slang_stage_to_vk_stage(ep_ref->getStage());
			std::string ep_name = ep_ref->getName();

			Slang::ComPtr<slang::IComponentType> linked;
			{
				Slang::ComPtr<slang::IBlob> diagnostics;
				composed->link(linked.writeRef(), diagnostics.writeRef());
				if (linked == nullptr) {
					error_log = (diagnostics != nullptr) ? static_cast<const char *>(diagnostics->getBufferPointer())
														 : "Link failed for entry point '" + ep_name + "'";
					out_stages.clear();
					return SlangCompileResult::Failed;
				}
			}

			Slang::ComPtr<slang::IBlob> spirv_blob;
			{
				Slang::ComPtr<slang::IBlob> diagnostics;
				linked->getEntryPointCode(0, 0, spirv_blob.writeRef(), diagnostics.writeRef());
				if (spirv_blob == nullptr) {
					error_log = (diagnostics != nullptr) ? static_cast<const char *>(diagnostics->getBufferPointer())
														 : "Code generation failed for entry point '" + ep_name + "'";
					out_stages.clear();
					return SlangCompileResult::Failed;
				}
			}

			VulkanCompiledStage result;
			result.stage = vk_stage;
			result.entry_point_name = "main"; // Slang always emits "main" in the SPIR-V binary
			result.linked_component = linked;
			result.session = session;
			const auto *data = static_cast<const Uint32 *>(spirv_blob->getBufferPointer());
			result.spirv.assign(data, data + (spirv_blob->getBufferSize() / sizeof(Uint32)));
			out_stages.push_back(std::move(result));
		}
		return SlangCompileResult::Ok;
	}

	/// Imports resolve in the engine shaders folder, then `search_paths`. Null on failure.
	[[nodiscard]] static Slang::ComPtr<slang::ISession> create_session(slang::IGlobalSession &global_session,
																	   const std::vector<std::string> &search_paths) {
		slang::TargetDesc target_desc{};
		target_desc.format = SLANG_SPIRV;
		target_desc.profile = global_session.findProfile(K_SPIRV_PROFILE);

		const std::string engine_shaders = Foundation::EnginePaths::shaders_dir();
		std::vector<const char *> paths = { engine_shaders.c_str() };
		for (const std::string &path : search_paths) {
			paths.push_back(path.c_str());
		}
		slang::SessionDesc session_desc{};
		session_desc.targets = &target_desc;
		session_desc.targetCount = 1;
		session_desc.searchPaths = paths.data();
		session_desc.searchPathCount = static_cast<SlangInt>(paths.size());

		Slang::ComPtr<slang::ISession> session;
		if (SLANG_FAILED(global_session.createSession(session_desc, session.writeRef()))) {
			return nullptr;
		}
		return session;
	}

	[[nodiscard]] static std::vector<ShaderPackStage> to_pack(const std::vector<VulkanCompiledStage> &stages) {
		std::vector<ShaderPackStage> out;
		out.reserve(stages.size());
		for (const VulkanCompiledStage &stage : stages) {
			out.push_back({ .stage = static_cast<Uint32>(stage.stage), .spirv = stage.spirv });
		}
		return out;
	}

	static void from_pack(const ShaderPackEntry &entry, std::vector<VulkanCompiledStage> &out_stages) {
		out_stages.clear();
		for (const ShaderPackStage &stage : entry.stages) {
			VulkanCompiledStage result;
			result.stage = static_cast<VkShaderStageFlagBits>(stage.stage);
			result.spirv = stage.spirv;
			result.entry_point_name = "main";
			out_stages.push_back(std::move(result));
		}
	}

	static VkShaderStageFlagBits slang_stage_to_vk_stage(SlangStage stage) {
		switch (stage) {
		case SLANG_STAGE_VERTEX:
			return VK_SHADER_STAGE_VERTEX_BIT;
		case SLANG_STAGE_FRAGMENT:
			return VK_SHADER_STAGE_FRAGMENT_BIT;
		case SLANG_STAGE_COMPUTE:
			return VK_SHADER_STAGE_COMPUTE_BIT;
		case SLANG_STAGE_GEOMETRY:
			return VK_SHADER_STAGE_GEOMETRY_BIT;
		default:
			AQUILA_LOG_WARNING("VulkanShaderCompiler: unhandled SlangStage {}, defaulting to VERTEX",
							   static_cast<int>(stage));
			return VK_SHADER_STAGE_VERTEX_BIT;
		}
	}

  private:
	static constexpr const char *K_SPIRV_PROFILE = "spirv_1_4";

	static inline Slang::ComPtr<slang::IGlobalSession> s_GlobalSession;
	static inline bool s_Initialized = false;
};

} // namespace Aquila::RHI
#endif
