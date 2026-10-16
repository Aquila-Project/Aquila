#ifndef SINGLETON_H
#define SINGLETON_H

#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/Defines.h"

namespace Aquila::Foundation {

void *&singleton_slot(std::string_view type_name);

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
