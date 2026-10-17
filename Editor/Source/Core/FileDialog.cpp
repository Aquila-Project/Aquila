#include "Core/FileDialog.h"

#include "Aquila/Foundation/Macros.h"

#include <nfd.h>

namespace Editor::FileDialog {

namespace {

class Session {
  public:
	Session() : m_ok(NFD_Init() == NFD_OKAY) {
		if (!m_ok) {
			AQUILA_LOG_ERROR("FileDialog: {}", NFD_GetError());
		}
	}
	~Session() {
		if (m_ok) {
			NFD_Quit();
		}
	}

	AQUILA_NONCOPYABLE(Session);
	AQUILA_NONMOVEABLE(Session);

	[[nodiscard]] bool ok() const { return m_ok; }

  private:
	bool m_ok;
};

std::vector<nfdu8filteritem_t> to_nfd(std::span<const Filter> filters) {
	std::vector<nfdu8filteritem_t> items;
	items.reserve(filters.size());
	for (const Filter &filter : filters) {
		items.push_back({ .name = filter.name.c_str(), .spec = filter.extensions.c_str() });
	}
	return items;
}

const char *or_null(const std::string &text) {
	return text.empty() ? nullptr : text.c_str();
}

Option<std::string> take(nfdresult_t result, nfdu8char_t *path) {
	if (result == NFD_OKAY) {
		std::string out(path);
		NFD_FreePathU8(path);
		return out;
	}
	if (result == NFD_ERROR) {
		AQUILA_LOG_ERROR("FileDialog: {}", NFD_GetError());
	}
	return std::nullopt;
}

}

Option<std::string> open_file(std::span<const Filter> filters, const std::string &directory) {
	const Session session;
	if (!session.ok()) {
		return std::nullopt;
	}
	const std::vector<nfdu8filteritem_t> items = to_nfd(filters);
	nfdu8char_t *path = nullptr;
	const nfdresult_t result =
		NFD_OpenDialogU8(&path, items.data(), static_cast<nfdfiltersize_t>(items.size()), or_null(directory));
	return take(result, path);
}

std::vector<std::string> open_files(std::span<const Filter> filters, const std::string &directory) {
	const Session session;
	if (!session.ok()) {
		return {};
	}
	const std::vector<nfdu8filteritem_t> items = to_nfd(filters);
	const nfdpathset_t *set = nullptr;
	const nfdresult_t result =
		NFD_OpenDialogMultipleU8(&set, items.data(), static_cast<nfdfiltersize_t>(items.size()), or_null(directory));
	if (result != NFD_OKAY) {
		if (result == NFD_ERROR) {
			AQUILA_LOG_ERROR("FileDialog: {}", NFD_GetError());
		}
		return {};
	}

	std::vector<std::string> paths;
	nfdpathsetsize_t count = 0;
	NFD_PathSet_GetCount(set, &count);
	for (nfdpathsetsize_t i = 0; i < count; ++i) {
		nfdu8char_t *path = nullptr;
		if (NFD_PathSet_GetPathU8(set, i, &path) == NFD_OKAY) {
			paths.emplace_back(path);
			NFD_PathSet_FreePathU8(path);
		}
	}
	NFD_PathSet_Free(set);
	return paths;
}

Option<std::string> save_file(std::span<const Filter> filters, const std::string &directory, const std::string &name) {
	const Session session;
	if (!session.ok()) {
		return std::nullopt;
	}
	const std::vector<nfdu8filteritem_t> items = to_nfd(filters);
	nfdu8char_t *path = nullptr;
	const nfdresult_t result = NFD_SaveDialogU8(&path, items.data(), static_cast<nfdfiltersize_t>(items.size()),
												or_null(directory), or_null(name));
	return take(result, path);
}

Option<std::string> pick_folder(const std::string &directory) {
	const Session session;
	if (!session.ok()) {
		return std::nullopt;
	}
	nfdu8char_t *path = nullptr;
	return take(NFD_PickFolderU8(&path, or_null(directory)), path);
}

}
