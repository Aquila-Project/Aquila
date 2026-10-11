#ifndef ABSTRACT_FILE_SYSTEM_H
#define ABSTRACT_FILE_SYSTEM_H

#include "Aquila/Foundation/Defines.h"
#include "Aquila/Platform/Filesystem/Files/NativeFile.h"

namespace Aquila::Platform::Filesystem {
class IFileSystem {
  public:
	AQUILA_NONCOPYABLE(IFileSystem);
	AQUILA_NONMOVEABLE(IFileSystem);

	IFileSystem() = default;
	virtual ~IFileSystem() = default;

	virtual Unique<NativeFile> file_open(const std::string &path, AccessMode access_mode, OpenMode open_mode) = 0;
	virtual bool file_exists(const std::string &path) = 0;
	virtual bool file_remove(const std::string &path) = 0;
	virtual bool file_move(const std::string &old_path, const std::string &new_path) = 0;
	virtual bool file_copy(const std::string &src_path, const std::string &dst_path) = 0;
	virtual Int64 file_get_size(const std::string &path) = 0;
	virtual Uint64 file_get_last_write_time(const std::string &path) = 0;

	// Directory operations
	virtual bool dir_exists(const std::string &path) = 0;
	virtual bool dir_create(const std::string &path) = 0;
	virtual bool dir_remove(const std::string &path) = 0;
	virtual bool dir_remove_all(const std::string &path) = 0;
	virtual std::vector<std::string> dir_list(const std::string &path) = 0;

	// Filesystem properties
	[[nodiscard]] virtual bool is_read_only() const = 0;
	[[nodiscard]] virtual std::string get_display_name() const = 0;
};
} // namespace Aquila::Platform::Filesystem

#endif // ABSTRACT_FILE_SYSTEM_H
