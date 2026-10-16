#include "Aquila/Foundation/Singleton.h"

namespace Aquila::Foundation {

void *&singleton_slot(std::string_view type_name) {
	static std::mutex mutex;
	static std::unordered_map<std::string, void *> slots;
	const std::scoped_lock lock(mutex);
	return slots[std::string(type_name)];
}

}
