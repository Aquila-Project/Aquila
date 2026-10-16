#ifndef AQUILA_IRHI_QUERY_POOL_H
#define AQUILA_IRHI_QUERY_POOL_H

#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Aquila::RHI {

class IRHIQueryPool {
  public:
	virtual ~IRHIQueryPool() = default;

	[[nodiscard]] virtual Uint32 get_count() const = 0;
	[[nodiscard]] virtual F64 get_nanoseconds_per_tick() const = 0;
	[[nodiscard]] virtual bool read_timestamps(Uint32 first, std::span<Uint64> out) = 0;
};

}

#endif
