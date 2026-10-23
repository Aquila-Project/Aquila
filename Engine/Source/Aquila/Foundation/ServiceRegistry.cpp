#include "Aquila/Foundation/ServiceRegistry.h"

#include "Aquila/Foundation/Macros.h"

namespace Aquila::Foundation {

Registration ServiceRegistry::provide_erased(std::string_view type_name, void *service) {
	const auto [it, inserted] = m_services.try_emplace(std::string(type_name), service);
	if (!inserted) {
		AQUILA_LOG_WARNING("ServiceRegistry: {} is already provided, keeping the first one", type_name);
		return {};
	}
	return m_owner.make([this, key = it->first, service] {
		if (const auto found = m_services.find(key); found != m_services.end() && found->second == service) {
			m_services.erase(found);
		}
	});
}

void *ServiceRegistry::find_erased(std::string_view type_name) const {
	const auto it = m_services.find(type_name);
	return it == m_services.end() ? nullptr : it->second;
}

}
