#ifndef SINGLETON_H
#define SINGLETON_H

#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/Defines.h"

namespace Aquila::Foundation {

void *&singleton_slot(std::string_view type_name);

/// The lifetime is explicit: init() before the first get(), shutdown() at the end.
/// get() asserts if it isn't initialized.
///
/// Instances live in one shared table keyed by type name, so every module sees the same one.
///
/// @code
/// class Thing : public Singleton<Thing> {
///     friend class Singleton<Thing>;
///     Thing() = default;
/// };
///
/// Thing::init();
/// Thing::get()->do_stuff();
/// Thing::shutdown();
/// @endcode
template <class T> class Singleton {
  public:
	template <typename... Args> static void init(Args &&...args) {
		void *&instance = slot();
		AQUILA_ASSERT(!instance, (std::string(typeid(T).name()) + ": Singleton already initialized").c_str());
		instance = new T(std::forward<Args>(args)...);
	}

	static T *get() {
		void *instance = slot();
		AQUILA_ASSERT(instance, (std::string(typeid(T).name()) + ": Singleton not initialized").c_str());
		return static_cast<T *>(instance);
	}

	static void shutdown() {
		void *&instance = slot();
		AQUILA_ASSERT(instance, (std::string(typeid(T).name()) + ": Singleton not initialized").c_str());
		delete static_cast<T *>(instance);
		instance = nullptr;
	}

  protected:
	Singleton() = default;
	~Singleton() = default;

  private:
	static void *&slot() {
		static void **cached = &singleton_slot(typeid(T).name());
		return *cached;
	}

	AQUILA_NONMOVEABLE(Singleton);
	AQUILA_NONCOPYABLE(Singleton);
};
} // namespace Aquila::Foundation

#endif // SINGLETON_H
