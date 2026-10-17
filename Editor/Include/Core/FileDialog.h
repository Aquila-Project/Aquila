#ifndef AQUILA_EDITOR_CORE_FILE_DIALOG_H
#define AQUILA_EDITOR_CORE_FILE_DIALOG_H

#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Editor::FileDialog {

/// `extensions` is comma separated without dots: { "Images", "png,jpg" }.
struct Filter {
	std::string name;
	std::string extensions;
};

/// Native OS dialogs (nativefiledialog-extended). They block until the user closes them, the editor doesn't render
/// meanwhile. Main thread only. Paths are native, nullopt/empty means cancelled or failed (failures are logged).
/// `directory` is where the dialog starts, empty lets the OS pick.
[[nodiscard]] Option<std::string> open_file(std::span<const Filter> filters = {}, const std::string &directory = {});
[[nodiscard]] std::vector<std::string> open_files(std::span<const Filter> filters = {},
												  const std::string &directory = {});
[[nodiscard]] Option<std::string> save_file(std::span<const Filter> filters = {}, const std::string &directory = {},
											const std::string &name = {});
[[nodiscard]] Option<std::string> pick_folder(const std::string &directory = {});

}

#endif
