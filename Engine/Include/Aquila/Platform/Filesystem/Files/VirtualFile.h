#ifndef VIRTUAL_FILE_H
#define VIRTUAL_FILE_H

#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Aquila::Platform::Filesystem {
class VirtualFile {
  public:
	virtual ~VirtualFile() = default;

	virtual size_t read(void *buffer, size_t size) = 0;
	virtual size_t write(const void *buffer, size_t size) = 0;
	virtual void close() = 0;

	/// `origin` is SEEK_SET, SEEK_CUR or SEEK_END.
	virtual bool seek(Int64 offset, int origin) = 0;
	[[nodiscard]] virtual Int64 tell() const = 0;
	[[nodiscard]] virtual Int64 size() const = 0;
	[[nodiscard]] virtual bool is_valid() const = 0;
};
} // namespace Aquila::Platform::Filesystem

#endif // VIRTUAL_FILE_H
