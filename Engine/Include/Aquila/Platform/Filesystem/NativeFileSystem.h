#ifndef AQUILA_NFS_H
#define AQUILA_NFS_H
#include "Aquila/Platform/Filesystem/IFileSystem.h"
#include "Aquila/Platform/Filesystem/Files/NativeFile.h"

namespace Aquila::Platform::Filesystem {

class NativeFileSystem final : public IFileSystem {
  private:
	std::string m_root_path;

	[[nodiscard]] std::string resolve_path(const std::string &path) const;
	static std::string to_fopen_mode(AccessMode access_mode, OpenMode open_mode);

  public:
	explicit NativeFileSystem(const std::string &root_path);
	~NativeFileSystem() override = default;

	// File operations
	Unique<NativeFile> file_open(const std::string &path, AccessMode access_mode, OpenMode open_mode) override;
	bool file_exists(const std::string &path) override;
	bool file_remove(const std::string &path) override;
	Int64 file_get_size(const std::string &path) override;
	Uint64 file_get_last_write_time(const std::string &path) override;

	bool file_move(const std::string &old_path, const std::string &new_path) override;

	bool file_copy(const std::string &src_path, const std::string &dst_path) override;

	// Directory operations
	bool dir_exists(const std::string &path) override;
	bool dir_create(const std::string &path) override;
	bool dir_remove(const std::string &path) override;
	bool dir_remove_all(const std::string &path) override;
	std::vector<std::string> dir_list(const std::string &path) override;

	[[nodiscard]] bool is_read_only() const override { return false; }
	[[nodiscard]] std::string get_display_name() const override { return "Native: " + m_root_path; }
};

} // namespace Aquila::Platform::Filesystem
#endif // AQUILA_NFS_H
