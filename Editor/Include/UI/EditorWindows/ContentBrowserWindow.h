#pragma once

#include "Core/EditorWindow.h"

namespace Aquila::UI::Core {
class PopupMenu;
class TextInput;
class View;
} // namespace Aquila::UI::Core

namespace Editor {

/// The open project's Content folder. It can't navigate above /project/Content.
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
	void open_item_menu(const std::string &path, Vec2 position);
	void create(const ContentCreator &creator);
	void begin_rename(const std::string &path);
	void rename(const std::string &path, const std::string &name);
	void confirm_delete(const std::string &path, Vec2 position);
	void remove(const std::string &path);
	/// Runs `action` at the start of the next update, for actions that rebuild the view that triggered them.
	void defer(Delegate<void()> action);

	Aquila::UI::Core::View *m_path = nullptr;
	Aquila::UI::Core::View *m_grid = nullptr;
	Aquila::UI::Core::TextInput *m_filter = nullptr;
	Aquila::UI::Core::PopupMenu *m_menu = nullptr;
	Aquila::UI::Core::PopupMenu *m_confirm = nullptr;
	std::vector<Delegate<void()>> m_deferred;
	std::vector<Tile> m_tiles;
	std::string m_directory;
	std::optional<std::string> m_pending;
	std::string m_selected;
	std::string m_rename_after_open;
	std::chrono::steady_clock::time_point m_last_press{};
};

} // namespace Editor
