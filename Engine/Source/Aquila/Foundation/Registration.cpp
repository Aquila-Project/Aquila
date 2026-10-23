#include "Aquila/Foundation/Registration.h"

namespace Aquila::Foundation {

Registration::Registration(std::weak_ptr<void> owner, Delegate<void()> unregister)
	: m_owner(std::move(owner)), m_unregister(std::move(unregister)) {}

Registration::~Registration() {
	reset();
}

Registration::Registration(Registration &&other) noexcept
	: m_owner(std::move(other.m_owner)), m_unregister(std::exchange(other.m_unregister, nullptr)) {}

Registration &Registration::operator=(Registration &&other) noexcept {
	if (this != &other) {
		reset();
		m_owner = std::move(other.m_owner);
		m_unregister = std::exchange(other.m_unregister, nullptr);
	}
	return *this;
}

void Registration::reset() {
	const Delegate<void()> unregister = std::exchange(m_unregister, nullptr);
	const std::shared_ptr<void> owner = m_owner.lock();
	m_owner.reset();
	if (owner && unregister) {
		unregister();
	}
}

bool Registration::is_active() const {
	return m_unregister && !m_owner.expired();
}

RegistrationOwner::RegistrationOwner() : m_token(std::make_shared<Uint8>(0)) {}

Registration RegistrationOwner::make(Delegate<void()> unregister) const {
	return { m_token, std::move(unregister) };
}

}
