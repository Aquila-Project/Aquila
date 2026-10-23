#ifndef AQUILA_FOUNDATION_SERVICE_REGISTRY_H
#define AQUILA_FOUNDATION_SERVICE_REGISTRY_H

#include "Aquila/Foundation/Registration.h"

namespace Aquila::Foundation {

/// One provider per type, keyed by type name like Singleton. Main thread only.
class ServiceRegistry {
  public:
	ServiceRegistry() = default;
	AQUILA_NONCOPYABLE(ServiceRegistry);
	AQUILA_NONMOVEABLE(ServiceRegistry);

	/// T isn't deduced. `service` must outlive the registration. Inactive when T is already provided.
	template <typename T> [[nodiscard]] Registration provide(std::type_identity_t<T> &service) {
		return provide_erased(typeid(T).name(), static_cast<T *>(&service));
	}

	/// Only services of modules added earlier are there during on_attach().
	template <typename T> [[nodiscard]] T *find() const { return static_cast<T *>(find_erased(typeid(T).name())); }

  private:
	Registration provide_erased(std::string_view type_name, void *service);
	[[nodiscard]] void *find_erased(std::string_view type_name) const;

	std::map<std::string, void *, std::less<>> m_services;
	RegistrationOwner m_owner;
};

}

#endif
