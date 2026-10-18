#ifndef AQUILA_VULKAN_SHADER_COMPILER_H
#define AQUILA_VULKAN_SHADER_COMPILER_H

#include "GraphicsPCH.h"
#include "Aquila/Foundation/EnginePaths.h"
#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"
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

/// Slang to SPIR-V. Call initialize() once first. Imports are looked up in the engine shaders folder.
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

	/// One stage per entry point in the file. All or nothing: if one entry point fails,
	/// `out_stages` is empty and `error_log` says why.
	/// @note Paths with "://" go through the VFS, everything else is read from disk.
	static bool compile_file(const std::string &filepath, std::vector<VulkanCompiledStage> &out_stages,
							std::string &error_log) {
		AQUILA_ASSERT(s_Initialized, "VulkanShaderCompiler::Initialize() was not called");

		slang::TargetDesc target_desc{};
		target_desc.format = SLANG_SPIRV;
		target_desc.profile = s_GlobalSession->findProfile(K_SPIRV_PROFILE);

		const char *search_paths[] = { Foundation::EnginePaths::shaders_dir().c_str() };
		slang::SessionDesc session_desc{};
		session_desc.targets = &target_desc;
		session_desc.targetCount = 1;
		session_desc.searchPaths = search_paths;
		session_desc.searchPathCount = 1;

		Slang::ComPtr<slang::ISession> session;
		if (SLANG_FAILED(s_GlobalSession->createSession(session_desc, session.writeRef()))) {
			error_log = "Failed to create Slang session";
			return false;
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

		slang::IModule *slang_module = nullptr;
		{
			Slang::ComPtr<slang::IBlob> diagnostics;
			slang_module = session->loadModuleFromSourceString(filepath.c_str(), filepath.c_str(), source_code.c_str(),
															  diagnostics.writeRef());
			if (slang_module == nullptr) {
				error_log = (diagnostics != nullptr) ? static_cast<const char *>(diagnostics->getBufferPointer())
													: "Failed to load Slang module from '" + filepath + "'";
				return false;
			}
		}

		Int32 entry_point_count = slang_module->getDefinedEntryPointCount();
		if (entry_point_count == 0) {
			error_log = "No entry points found in '" + filepath + "'";
			return false;
		}

		bool any_failed = false;
		for (Int32 ep = 0; ep < entry_point_count; ++ep) {
			Slang::ComPtr<slang::IEntryPoint> entry_point;
			if (SLANG_FAILED(slang_module->getDefinedEntryPoint(ep, entry_point.writeRef()))) {
				AQUILA_LOG_WARNING("VulkanShaderCompiler: failed to get entry point {} from '{}'", ep, filepath);
				any_failed = true;
				continue;
			}

			slang::IComponentType *components[] = { slang_module, entry_point };
			Slang::ComPtr<slang::IComponentType> composed;
			session->createCompositeComponentType(components, 2, composed.writeRef());

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
					any_failed = true;
					continue;
				}
			}

			Slang::ComPtr<slang::IBlob> spirv_blob;
			{
				Slang::ComPtr<slang::IBlob> diagnostics;
				linked->getEntryPointCode(0, 0, spirv_blob.writeRef(), diagnostics.writeRef());
				if (spirv_blob == nullptr) {
					error_log = (diagnostics != nullptr) ? static_cast<const char *>(diagnostics->getBufferPointer())
														: "Code generation failed for entry point '" + ep_name + "'";
					any_failed = true;
					continue;
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

		if (out_stages.empty()) {
			if (error_log.empty()) {
				error_log = "All entry points failed to compile in '" + filepath + "'";
			}
			return false;
		}

		if (any_failed) {
			out_stages.clear();
			return false;
		}

		return true;
	}

	/// Same as compile_file() for source in memory. `name` is the module name used in errors.
	static bool compile_source(const std::string &name, const std::string &source,
							  std::vector<VulkanCompiledStage> &out_stages, std::string &error_log) {
		AQUILA_ASSERT(s_Initialized, "VulkanShaderCompiler::Initialize() was not called");

		slang::TargetDesc target_desc{};
		target_desc.format = SLANG_SPIRV;
		target_desc.profile = s_GlobalSession->findProfile(K_SPIRV_PROFILE);

		const char *search_paths[] = { Foundation::EnginePaths::shaders_dir().c_str() };
		slang::SessionDesc session_desc{};
		session_desc.targets = &target_desc;
		session_desc.targetCount = 1;
		session_desc.searchPaths = search_paths;
		session_desc.searchPathCount = 1;

		Slang::ComPtr<slang::ISession> session;
		if (SLANG_FAILED(s_GlobalSession->createSession(session_desc, session.writeRef()))) {
			error_log = "Failed to create Slang session";
			return false;
		}

		slang::IModule *slang_module = nullptr;
		{
			Slang::ComPtr<slang::IBlob> diagnostics;
			slang_module =
				session->loadModuleFromSourceString(name.c_str(), name.c_str(), source.c_str(), diagnostics.writeRef());
			if (slang_module == nullptr) {
				error_log = (diagnostics != nullptr) ? static_cast<const char *>(diagnostics->getBufferPointer())
													: "Failed to load Slang module '" + name + "'";
				return false;
			}
		}

		Int32 entry_point_count = slang_module->getDefinedEntryPointCount();
		if (entry_point_count == 0) {
			error_log = "No entry points found in '" + name + "'";
			return false;
		}

		bool any_failed = false;
		for (Int32 ep = 0; ep < entry_point_count; ++ep) {
			Slang::ComPtr<slang::IEntryPoint> entry_point;
			if (SLANG_FAILED(slang_module->getDefinedEntryPoint(ep, entry_point.writeRef()))) {
				any_failed = true;
				continue;
			}

			slang::IComponentType *components[] = { slang_module, entry_point };
			Slang::ComPtr<slang::IComponentType> composed;
			session->createCompositeComponentType(components, 2, composed.writeRef());

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
					any_failed = true;
					continue;
				}
			}

			Slang::ComPtr<slang::IBlob> spirv_blob;
			{
				Slang::ComPtr<slang::IBlob> diagnostics;
				linked->getEntryPointCode(0, 0, spirv_blob.writeRef(), diagnostics.writeRef());
				if (spirv_blob == nullptr) {
					error_log = (diagnostics != nullptr) ? static_cast<const char *>(diagnostics->getBufferPointer())
														: "Code gen failed for entry point '" + ep_name + "'";
					any_failed = true;
					continue;
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

		if (out_stages.empty() || any_failed) {
			out_stages.clear();
			return false;
		}
		return true;
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
