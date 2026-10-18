#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"
#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Aquila::Platform::Filesystem {

std::string VirtualFileSystem::normalize_path(const std::string &path) {
	if (path.empty()) {
		return "/";
	}

	std::string normalized = path;

	std::ranges::replace(normalized, '\\', '/');

	if (normalized[0] != '/') {
		normalized = "/" + normalized;
	}

	if (normalized.length() > 1 && normalized.back() == '/') {
		normalized.pop_back();
	}

	return normalized;
}

MountPoint *VirtualFileSystem::find_mount_point(const std::string &virtual_path, std::string &relative_path) {
	std::shared_lock<std::shared_mutex> lock(m_mount_points_mutex);

	std::string normalized_path = normalize_path(virtual_path);

	for (auto &mount : m_mount_points) {
		const std::string &mount_path = mount.virtual_path;

		if (normalized_path.starts_with(mount_path)) {
			if (normalized_path.length() == mount_path.length()) {
				relative_path = "/";
			} else if (normalized_path[mount_path.length()] == '/') {
				relative_path = normalized_path.substr(mount_path.length());
			} else {
				continue;
			}

			return &mount;
		}
	}

	return nullptr;
}

bool VirtualFileSystem::mount(const std::string &virtual_path, Ref<IFileSystem> file_system, int priority,
							  bool read_only) {
	if (!file_system) {
		return false;
	}

	std::string normalized_path = normalize_path(virtual_path);

	std::unique_lock<std::shared_mutex> lock(m_mount_points_mutex);

	for (const auto &mount : m_mount_points) {
		if (mount.virtual_path == normalized_path) {
			return false;
		}
	}

	MountPoint mount;
	mount.virtual_path = normalized_path;
	mount.file_system = file_system;
	mount.priority = priority;
	mount.read_only = read_only;

	m_mount_points.push_back(mount);

	std::ranges::sort(m_mount_points, [](const MountPoint &a, const MountPoint &b) {
		if (a.priority != b.priority) {
			return a.priority > b.priority;
		}
		return a.virtual_path.length() > b.virtual_path.length();
	});
	return true;
}

bool VirtualFileSystem::unmount(const std::string &virtual_path) {
	std::string normalized_path = normalize_path(virtual_path);

	std::unique_lock<std::shared_mutex> lock(m_mount_points_mutex);

	auto it = std::ranges::find_if(
		m_mount_points, [&normalized_path](const MountPoint &mount) { return mount.virtual_path == normalized_path; });

	if (it != m_mount_points.end()) {
		m_mount_points.erase(it);
		return true;
	}

	return false;
}

Unique<VirtualFile> VirtualFileSystem::open_file(const std::string &virtual_path, AccessMode access_mode,
												 OpenMode open_mode) {
	std::string relative_path;
	MountPoint *mount = find_mount_point(virtual_path, relative_path);
	if (mount == nullptr) {
		return nullptr;
	}

	bool is_writing = access_mode == AccessMode::Write || access_mode == AccessMode::ReadWrite ||
		has_flag(open_mode, OpenMode::Append);

	if (is_writing && (mount->read_only || mount->file_system->is_read_only())) {
		return nullptr;
	}

	return mount->file_system->file_open(relative_path, access_mode, open_mode);
}
bool VirtualFileSystem::exists(const std::string &virtual_path) {
	std::string relative_path;
	MountPoint *mount = find_mount_point(virtual_path, relative_path);

	if (mount == nullptr) {
		return false;
	}
	return mount->file_system->file_exists(relative_path);
}

std::vector<std::string> VirtualFileSystem::list_directory(const std::string &virtual_path) {
	std::string relative_path;
	MountPoint *mount = find_mount_point(virtual_path, relative_path);

	if (mount == nullptr) {
		return {};
	}
	return mount->file_system->dir_list(relative_path);
}

bool VirtualFileSystem::is_directory(const std::string &virtual_path) {
	std::string relative_path;
	MountPoint *mount = find_mount_point(virtual_path, relative_path);

	if (mount == nullptr) {
		return false;
	}
	return mount->file_system->dir_exists(relative_path);
}

Int64 VirtualFileSystem::get_file_size(const std::string &virtual_path) {
	std::string relative_path;
	MountPoint *mount = find_mount_point(virtual_path, relative_path);

	if (mount == nullptr) {
		return -1;
	}
	return mount->file_system->file_get_size(relative_path);
}

Uint64 VirtualFileSystem::get_last_write_time(const std::string &virtual_path) {
	std::string relative_path;
	MountPoint *mount = find_mount_point(virtual_path, relative_path);

	if (mount == nullptr) {
		return 0;
	}
	return mount->file_system->file_get_last_write_time(relative_path);
}

bool VirtualFileSystem::create_dir(const std::string &virtual_path) {
	std::string relative_path;
	MountPoint *mount = find_mount_point(virtual_path, relative_path);

	if ((mount == nullptr) || mount->read_only || mount->file_system->is_read_only()) {
		return false;
	}

	return mount->file_system->dir_create(relative_path);
}

bool VirtualFileSystem::delete_file_aq(const std::string &virtual_path) {
	std::string relative_path;
	MountPoint *mount = find_mount_point(virtual_path, relative_path);

	if ((mount == nullptr) || mount->read_only || mount->file_system->is_read_only()) {
		return false;
	}

	return mount->file_system->file_remove(relative_path);
}

bool VirtualFileSystem::delete_directory(const std::string &virtual_path, bool recursive) {
	std::string relative_path;
	MountPoint *mount = find_mount_point(virtual_path, relative_path);

	if ((mount == nullptr) || mount->read_only || mount->file_system->is_read_only()) {
		return false;
	}

	return recursive ? mount->file_system->dir_remove_all(relative_path)
					 : mount->file_system->dir_remove(relative_path);
}

bool VirtualFileSystem::rename_file(const std::string &old_virtual_path, const std::string &new_virtual_path) {
	std::string old_relative_path;
	MountPoint *old_mount = find_mount_point(old_virtual_path, old_relative_path);

	std::string new_relative_path;
	MountPoint *new_mount = find_mount_point(new_virtual_path, new_relative_path);

	// Both paths must be on the same mount point
	if ((old_mount == nullptr) || (new_mount == nullptr) || old_mount != new_mount) {
		return false;
	}

	if (old_mount->read_only || old_mount->file_system->is_read_only()) {
		return false;
	}

	return old_mount->file_system->file_move(old_relative_path, new_relative_path);
}

bool VirtualFileSystem::copy_file_a(const std::string &src_virtual_path, const std::string &dst_virtual_path) {
	std::string src_relative_path;
	MountPoint *src_mount = find_mount_point(src_virtual_path, src_relative_path);

	std::string dst_relative_path;
	MountPoint *dst_mount = find_mount_point(dst_virtual_path, dst_relative_path);

	if ((src_mount == nullptr) || (dst_mount == nullptr)) {
		return false;
	}

	if (dst_mount->read_only || dst_mount->file_system->is_read_only()) {
		return false;
	}

	// If same mount, use native copy if available
	if (src_mount == dst_mount) {
		return src_mount->file_system->file_copy(src_relative_path, dst_relative_path);
	}

	// Otherwise, read from source and write to destination
	auto src_file = src_mount->file_system->file_open(src_relative_path, AccessMode::Read, OpenMode::Binary);
	if (!src_file) {
		return false;
	}

	std::vector<Uint8> buffer(src_file->size());
	if (src_file->read(buffer.data(), buffer.size()) != buffer.size()) {
		return false;
	}

	auto dst_file =
		dst_mount->file_system->file_open(dst_relative_path, AccessMode::Write, OpenMode::Binary | OpenMode::Truncate);
	if (!dst_file) {
		return false;
	}

	return dst_file->write(buffer.data(), buffer.size()) == buffer.size();
}

bool VirtualFileSystem::write_text_file(const std::string &virtual_path, const std::string &content) {
	std::string relative_path;
	MountPoint *mount = find_mount_point(virtual_path, relative_path);

	if ((mount == nullptr) || mount->read_only || mount->file_system->is_read_only()) {
		return false;
	}

	auto file = mount->file_system->file_open(relative_path, AccessMode::Write, OpenMode::Text);
	if (!file) {
		return false;
	}

	if (content.empty()) {
		return true;
	}

	return file->write(reinterpret_cast<const Uint8 *>(content.data()), content.size()) == content.size();
}

Option<std::string> VirtualFileSystem::to_native_path(const std::string &virtual_path) {
	std::string relative_path;
	MountPoint *mount = find_mount_point(virtual_path, relative_path);
	if (mount == nullptr) {
		return std::nullopt;
	}
	return mount->file_system->native_path(relative_path);
}

std::string VirtualFileSystem::read_text_file(const std::string &virtual_path) {
	std::string relative_path;
	MountPoint *mount = find_mount_point(virtual_path, relative_path);

	if (mount == nullptr) {
		return "";
	}

	auto file = mount->file_system->file_open(relative_path, AccessMode::Read, OpenMode::Text);
	if (!file) {
		return "";
	}

	const Int64 size = file->size();
	if (size <= 0) {
		return "";
	}

	std::string result(static_cast<Usize>(size), '\0');
	const Usize bytes_read = file->read(reinterpret_cast<Uint8 *>(result.data()), static_cast<Usize>(size));
	result.resize(bytes_read);
	return result;
}

std::vector<std::string> VirtualFileSystem::get_mount_points() const {
	std::shared_lock<std::shared_mutex> lock(m_mount_points_mutex);

	std::vector<std::string> result;
	result.reserve(m_mount_points.size());
	for (const auto &mount : m_mount_points) {
		result.push_back(mount.virtual_path);
	}
	return result;
}

bool VirtualFileSystem::is_mounted(const std::string &virtual_path) const {
	SharedLock lock(m_mount_points_mutex);

	std::string normalized_path = const_cast<VirtualFileSystem *>(this)->normalize_path(virtual_path);
	for (const auto &mount : m_mount_points) {
		if (mount.virtual_path == normalized_path) {
			return true;
		}
	}
	return false;
}

void VirtualFileSystem::unmount_all() {
	MutexLock lock(m_mount_points_mutex);
	m_mount_points.clear();
}

} // namespace Aquila::Platform::Filesystem
