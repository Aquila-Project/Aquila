#ifndef AQUILA_FOUNDATION_REGISTRATION_H
#define AQUILA_FOUNDATION_REGISTRATION_H

#include "Aquila/Foundation/Defines.h"
#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Aquila::Foundation {

class RegistrationOwner;

/// Unregisters when destroyed or reset(). Does nothing once the registry is gone. Main thread only.
class Registration {
  public:
	Registration() = default;
	~Registration();

	Registration(Registration &&other) noexcept;
	Registration &operator=(Registration &&other) noexcept;
	AQUILA_NONCOPYABLE(Registration);

	void reset();
	[[nodiscard]] bool is_active() const;

  private:
	friend class RegistrationOwner;

	Registration(std::weak_ptr<void> owner, Delegate<void()> unregister);

	std::weak_ptr<void> m_owner;
	Delegate<void()> m_unregister;
};

/// Make it the registry's last member, so it dies first and late resets do nothing.
class RegistrationOwner {
  public:
	RegistrationOwner();
	AQUILA_NONCOPYABLE(RegistrationOwner);
	AQUILA_NONMOVEABLE(RegistrationOwner);

	/// `unregister` runs at most once.
	[[nodiscard]] Registration make(Delegate<void()> unregister) const;

  private:
	std::shared_ptr<Uint8> m_token;
};

}

#endif
