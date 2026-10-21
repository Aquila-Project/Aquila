#include "Aquila/Graphics/Shader/ShaderBaker.h"
#include "Aquila/Foundation/SharedConstants.h"
#include "Aquila/Graphics/Shader/ShaderProgram.h"
#include "Aquila/Graphics/Shader/ShaderSource.h"
#include "Aquila/RHI/ShaderPack.h"
#include "Aquila/RHI/Vulkan/VulkanShaderCompiler.h"

namespace Aquila::Graphics::Shader {

namespace {

namespace fs = std::filesystem;

std::vector<fs::path> find_shaders(const fs::path &root) {
	std::vector<fs::path> files;
	std::error_code ec;
	if (!fs::is_directory(root, ec)) {
		return files;
	}
	auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec);
	for (; !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
		const fs::path &path = it->path();
		if (it->is_directory(ec)) {
			if (path.filename().string().starts_with('.')) {
				it.disable_recursion_pending();
			}
			continue;
		}
		if (path.extension() == ".slang" && it->is_regular_file(ec)) {
			files.push_back(path);
		}
	}
	std::ranges::sort(files);
	return files;
}

bool read_text(const fs::path &path, std::string &out) {
	std::ifstream file(path, std::ios::binary);
	if (!file) {
		return false;
	}
	out.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
	return true;
}

class Baker {
  public:
	Baker() : m_global(RHI::VulkanShaderCompiler::create_global_session()) {}

	RHI::SlangCompileResult bake(const std::string &key, const std::string &name, const std::string &code,
								 const std::string &directory, ShaderBakeReport &report) {
		const ShaderDomain domain = detect_shader_domain(code);
		for (const ShaderVariant variant : domain_variants(domain)) {
			ShaderSource source;
			std::string error;
			if (!compose_shader_source(name, code, directory, variant, source, error)) {
				report.errors.push_back(key + ": " + error);
				return RHI::SlangCompileResult::Failed;
			}

			std::vector<RHI::VulkanCompiledStage> compiled;
			const RHI::SlangCompileInput input{
				.name = source.name,
				.source = source.code,
				.search_paths = source.search_paths,
				.entry_module = source.entry_module,
			};
			const RHI::SlangCompileResult result =
				RHI::VulkanShaderCompiler::compile(*m_global, input, compiled, error);
			if (result == RHI::SlangCompileResult::NoEntryPoints && domain == ShaderDomain::Custom) {
				return result;
			}
			if (result != RHI::SlangCompileResult::Ok) {
				report.errors.push_back(key + ": " + error);
				return RHI::SlangCompileResult::Failed;
			}

			RHI::ShaderPackEntry entry;
			entry.stages = RHI::VulkanShaderCompiler::to_pack(compiled);
			const std::vector<ShaderStage> stages = ShaderProgram::make_stages(compiled);
			entry.metadata = ShaderProgram::write_metadata(domain, ShaderProgram::reflect_stages(stages));
			m_pack.add(RHI::shader_key(key, variant_name(variant)), std::move(entry));
			++report.programs;
		}
		return RHI::SlangCompileResult::Ok;
	}

	void write_modules(const fs::path &source_dir, const std::vector<std::string> &modules, const fs::path &out_dir,
					   ShaderBakeReport &report) {
		if (modules.empty()) {
			return;
		}
		std::string source;
		for (std::string module : modules) {
			std::ranges::replace(module, '/', '.');
			source += "import " + module + ";\n";
		}

		Slang::ComPtr<slang::ISession> session = RHI::VulkanShaderCompiler::create_session(*m_global, {});
		if (session == nullptr) {
			report.errors.emplace_back("modules: failed to create a Slang session");
			return;
		}
		const std::string root_path = (source_dir / "AquilaBakedModules.slang").string();
		Slang::ComPtr<slang::IBlob> diagnostics;
		slang::IModule *root = session->loadModuleFromSourceString("AquilaBakedModules", root_path.c_str(),
																   source.c_str(), diagnostics.writeRef());
		if (root == nullptr) {
			report.errors.push_back(std::string("modules: ") +
									(diagnostics != nullptr ? static_cast<const char *>(diagnostics->getBufferPointer())
															: "failed to import them"));
			return;
		}

		for (SlangInt i = 0; i < session->getLoadedModuleCount(); ++i) {
			slang::IModule *module = session->getLoadedModule(i);
			if (module == root || module->getFilePath() == nullptr) {
				continue;
			}
			const fs::path relative = fs::path(module->getFilePath()).lexically_relative(source_dir);
			if (relative.empty() || *relative.begin() == "..") {
				continue;
			}
			fs::path target = out_dir / relative;
			target.replace_extension(".slang-module");
			std::error_code ec;
			fs::create_directories(target.parent_path(), ec);
			if (SLANG_FAILED(module->writeToFile(target.string().c_str()))) {
				report.errors.push_back(relative.generic_string() + ": can't write '" + target.string() + "'");
				continue;
			}
			++report.modules;
		}
	}

	void save(const std::string &path, ShaderBakeReport &report) const {
		std::string error;
		if (!m_pack.save(path, error)) {
			report.errors.push_back(error);
		}
	}

	[[nodiscard]] Usize size() const { return m_pack.size(); }

  private:
	Slang::ComPtr<slang::IGlobalSession> m_global;
	RHI::ShaderPack m_pack;
};

}

ShaderBakeReport bake_engine_shaders(const std::string &source_dir, const std::string &out_dir,
									 std::span<const std::string> skip) {
	ShaderBakeReport report;
	Baker baker;
	std::vector<std::string> modules;
	const std::span<const std::string_view> entry = entry_modules();

	for (const fs::path &file : find_shaders(source_dir)) {
		const std::string key = file.lexically_relative(source_dir).generic_string();
		if (std::ranges::find(skip, key) != skip.end()) {
			continue;
		}
		std::string module = key.substr(0, key.size() - std::string_view(".slang").size());
		if (std::ranges::find(entry, module) != entry.end()) {
			modules.push_back(std::move(module));
			continue;
		}

		std::string code;
		if (!read_text(file, code)) {
			report.errors.push_back(key + ": can't read it");
			continue;
		}
		const std::string directory = file.parent_path().string();
		if (baker.bake(key, file.string(), code, directory, report) == RHI::SlangCompileResult::NoEntryPoints) {
			modules.push_back(std::move(module));
		}
	}

	if (report.ok()) {
		baker.write_modules(source_dir, modules, out_dir, report);
	}
	if (report.ok()) {
		baker.save((fs::path(out_dir) / RHI::ENGINE_SHADER_PACK).string(), report);
	}
	return report;
}

ShaderBakeReport bake_project_shaders(const std::string &project_dir, const std::string &pack_path) {
	ShaderBakeReport report;
	const fs::path content = fs::path(project_dir) / SharedConstants::PROJECT_CONTENT_FOLDER;
	const std::vector<fs::path> files = find_shaders(content);
	if (files.empty()) {
		return report;
	}

	Baker baker;
	for (const fs::path &file : files) {
		const std::string name =
			SharedConstants::PROJECT_MOUNT + "/" + file.lexically_relative(project_dir).generic_string();
		std::string code;
		if (!read_text(file, code)) {
			report.errors.push_back(name + ": can't read it");
			continue;
		}
		baker.bake(name, name, code, file.parent_path().string(), report);
	}

	if (report.ok() && baker.size() > 0) {
		baker.save(pack_path, report);
	}
	return report;
}

}
