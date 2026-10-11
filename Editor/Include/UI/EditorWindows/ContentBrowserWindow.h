#pragma once

#include "Core/EditorWindow.h"

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace Aquila::UI::Core {
class PopupMenu;
class TextInput;
class View;
}

namespace Editor {

class ContentBrowserWindow final : public EditorWindow {
  public:
	explicit ContentBrowserWindow(EditorContext &context);

	void build(Aquila::UI::Core::View &content) override;
	void update(F32 delta_time) override;

	void navigate(std::string directory);

  private:
	struct Tile {
		Aquila::UI::Core::View *view = nullptr;
		std::string name;
		std::string path;
		bool is_directory = false;
	};

	void open(const std::string &directory);
	void build_path();
	void build_grid();
	void apply_filter();
	void press(const std::string &path);
	void open_background_menu(Vec2 position);
	void create(const ContentCreator &creator);

	Aquila::UI::Core::View *m_path = nullptr;
	Aquila::UI::Core::View *m_grid = nullptr;
	Aquila::UI::Core::TextInput *m_filter = nullptr;
	Aquila::UI::Core::PopupMenu *m_menu = nullptr;
	std::vector<Tile> m_tiles;
	std::string m_directory;
	std::optional<std::string> m_pending;
	std::string m_selected;
	std::chrono::steady_clock::time_point m_last_press{};
};

}
