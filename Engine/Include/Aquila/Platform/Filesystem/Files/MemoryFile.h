#ifndef MEMORY_FILE_H
#define MEMORY_FILE_H

#include "Aquila/Platform/Filesystem/Files/VirtualFile.h"

namespace Aquila::Platform::Filesystem {
/// A file over a byte vector. It doesn't own the vector, keep it alive while the file is open.
class MemoryFile final : public VirtualFile {
  private:
	std::vector<Uint8> *m_data;
	Usize m_position;
	bool m_can_write;

  public:
	MemoryFile(std::vector<Uint8> *data, bool can_write);
	Usize read(void *buffer, Usize size) override;
	Usize write(const void *buffer, Usize size) override;
	bool seek(Int64 offset, int origin) override;
	[[nodiscard]] Int64 tell() const override;
	[[nodiscard]] Int64 size() const override;
	[[nodiscard]] bool is_valid() const override;

	void close() override;
};
} // namespace Aquila::Platform::Filesystem

#endif // MEMORY_FILE_H
