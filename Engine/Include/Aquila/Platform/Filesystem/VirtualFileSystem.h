#ifndef AQUILA_VFS_H
#define AQUILA_VFS_H

#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Platform/Filesystem/IFileSystem.h"
#include "Aquila/Foundation/Singleton.h"

namespace Aquila::Platform::Filesystem {

struct MountPoint {
	std::string virtual_path;
	std::string real_path;
	Ref<IFileSystem> file_system;
	int priority = 0;
	bool read_only = false;
};

/// Maps virtual paths like /project/Content/a.png to file systems mounted on a prefix.
///
/// @code
/// auto vfs = VirtualFileSystem::get();
/// vfs->mount("/project", std::make_shared<NativeFileSystem>(project_dir));
/// std::string text = vfs->read_text_file("/project/Content/notes.txt");
/// @endcode
class VirtualFileSystem : public Foundation::Singleton<VirtualFileSystem> {
	friend class Singleton;

  private:
	std::vector<MountPoint> m_mount_points;
	mutable Mutex m_mount_points_mutex;

	std::string normalize_path(const std::string &path);
	MountPoint *find_mount_point(const std::string &virtual_path, std::string &relative_path);

  public:
	/// Higher priority wins, then the longest prefix. The first match is used even if the file
	/// isn't there, nothing falls through to other mounts.
	/// @return false when `virtual_path` is already mounted
	bool mount(const std::string &virtual_path, Ref<IFileSystem> file_system, int priority = 0, bool read_only = false);
	bool unmount(const std::string &virtual_path);
	void unmount_all();
	/// Both paths must be on the same mount.
	bool rename_file(const std::string &old_virtual_path, const std::string &new_virtual_path);
	Unique<VirtualFile> open_file(const std::string &virtual_path, AccessMode access_mode, OpenMode open_mode);
	bool exists(const std::string &virtual_path);
	/// Names only, not full paths.
	std::vector<std::string> list_directory(const std::string &virtual_path);
	bool is_directory(const std::string &virtual_path);
	Int64 get_file_size(const std::string &virtual_path);
	Uint64 get_last_write_time(const std::string &virtual_path);

	bool copy_file_a(const std::string &src_virtual_path, const std::string &dst_virtual_path);
	bool create_dir(const std::string &virtual_path);
	bool delete_file_aq(const std::string &virtual_path);
	bool delete_directory(const std::string &virtual_path, bool recursive = false);
	bool write_text_file(const std::string &virtual_path, const std::string &content);
	/// Empty on failure, you can't tell it apart from an empty file.
	std::string read_text_file(const std::string &virtual_path);
	std::vector<std::string> get_mount_points() const;
	bool is_mounted(const std::string &virtual_path) const;
};

} // namespace Aquila::Platform::Filesystem

#endif // VFS_H
