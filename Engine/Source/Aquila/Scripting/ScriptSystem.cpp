#include "Aquila/Scripting/ScriptSystem.h"

#include "Aquila/Foundation/Macros.h"
#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"
#include "Aquila/Scripting/ScriptBindings.h"
#include "Aquila/Scene/Components/ScriptComponent.h"

namespace Aquila::Scripting {

namespace {

ScriptEntity to_script(entt::entity entity) {
	return ScriptEntity{ entt::to_integral(entity) };
}

} // namespace

ScriptSystem::ScriptSystem(SceneManagement::Scene &scene, const Platform::InputState *input)
	: m_scene(scene), m_environment{ .scene = &scene, .input = input } {
	m_runtime.set_error_handler([](std::string_view message) { AQUILA_LOG_ERROR("[Luau] {}", message); });
	register_engine_api(m_runtime, m_environment);
}

ScriptSystem::~ScriptSystem() = default;

bool ScriptSystem::write_types(const std::string &directory) const {
	return m_runtime.write_types(directory);
}

void ScriptSystem::update(F32 delta_time) {
	m_environment.delta_time = delta_time;
	m_environment.elapsed += delta_time;
	++m_environment.frame;

	auto &registry = m_scene.get_registry();

	for (auto it = m_instances.begin(); it != m_instances.end();) {
		const bool alive = registry.valid(it->first);
		if (alive && registry.all_of<SceneManagement::Components::ScriptComponent>(it->first)) {
			++it;
			continue;
		}
		if (alive) {
			finish(it->first, it->second);
		}
		m_runtime.unload(it->second.script);
		it = m_instances.erase(it);
	}

	for (auto [entity, component] : registry.view<SceneManagement::Components::ScriptComponent>().each()) {
		Instance &instance = m_instances[entity];
		if (instance.path != component.path) {
			start(entity, instance, component.path);
		}
	}

	for (auto &[entity, instance] : m_instances) {
		if (instance.has_update && !instance.failed) {
			instance.failed = !m_runtime.call(instance.script, "on_update", to_script(entity), delta_time);
		}
	}
}

void ScriptSystem::stop() {
	auto &registry = m_scene.get_registry();
	for (auto &[entity, instance] : m_instances) {
		if (registry.valid(entity)) {
			finish(entity, instance);
		}
		m_runtime.unload(instance.script);
	}
	m_instances.clear();
}

void ScriptSystem::finish(entt::entity entity, Instance &instance) {
	if (instance.has_stop && !instance.failed) {
		m_runtime.call(instance.script, "on_stop", to_script(entity));
	}
}

void ScriptSystem::start(entt::entity entity, Instance &instance, const std::string &path) {
	finish(entity, instance);
	m_runtime.unload(instance.script);
	instance = Instance{};
	instance.path = path;
	if (path.empty()) {
		return;
	}

	auto *vfs = Platform::Filesystem::VirtualFileSystem::get();
	if (!vfs->exists(path)) {
		AQUILA_LOG_ERROR("Script not found: {}", path);
		return;
	}

	instance.script = m_runtime.load_source(path, vfs->read_text_file(path));
	if (!instance.script.is_valid()) {
		return;
	}

	instance.has_update = m_runtime.has_function(instance.script, "on_update");
	instance.has_stop = m_runtime.has_function(instance.script, "on_stop");
	if (m_runtime.has_function(instance.script, "on_start")) {
		instance.failed = !m_runtime.call(instance.script, "on_start", to_script(entity));
	}
}

} // namespace Aquila::Scripting
