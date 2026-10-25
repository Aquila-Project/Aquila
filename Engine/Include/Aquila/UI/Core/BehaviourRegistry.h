#ifndef AQUILA_UI_CORE_BEHAVIOUR_REGISTRY_H
#define AQUILA_UI_CORE_BEHAVIOUR_REGISTRY_H

#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/Registration.h"
#include "Aquila/UI/Core/Behaviour.h"

namespace Aquila::UI::Core {

class BehaviourRegistry {
  public:
	using Factory = Delegate<Unique<Behaviour>()>;

	BehaviourRegistry();
	~BehaviourRegistry();

	AQUILA_NONCOPYABLE(BehaviourRegistry);
	AQUILA_NONMOVEABLE(BehaviourRegistry);

	[[nodiscard]] Foundation::Registration add(std::string name, Factory factory);

	template <std::derived_from<Behaviour> T> [[nodiscard]] Foundation::Registration add(std::string name) {
		return add(std::move(name), [] { return Unique<Behaviour>(std::make_unique<T>()); });
	}

	[[nodiscard]] Unique<Behaviour> create(std::string_view name) const;

  private:
	std::map<std::string, Factory, std::less<>> m_factories;
	Foundation::RegistrationOwner m_owner;
};

}

#endif
