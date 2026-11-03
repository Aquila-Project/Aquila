#include "Aquila/Scripting/ScriptApi.h"

namespace Aquila::Scripting {

Foundation::Registration ScriptApi::add(Binder binder) {
	const Uint64 id = m_next_id++;
	m_binders.push_back({ .id = id, .binder = std::move(binder) });
	++m_revision;
	return m_owner.make([this, id] {
		std::erase_if(m_binders, [id](const Entry &entry) { return entry.id == id; });
		++m_revision;
	});
}

void ScriptApi::bind(aquiLua::Runtime &runtime, ScriptEnvironment &environment) const {
	register_engine_api(runtime, environment);
	for (const Entry &entry : m_binders) {
		entry.binder(runtime, environment);
	}
}

}
