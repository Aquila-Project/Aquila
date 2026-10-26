#ifndef AQUILA_UI_STYLE_STYLE_SET_H
#define AQUILA_UI_STYLE_STYLE_SET_H

#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Aquila::UI {

class StyleSet {
  public:
	void set_base(std::vector<std::string> paths);
	void note_layout(const std::string &layout_path, bool has_root, std::vector<std::string> imports);
	[[nodiscard]] std::vector<std::string> get_paths() const;
	[[nodiscard]] Uint64 get_revision() const { return m_revision; }
	void touch() { ++m_revision; }

  private:
	struct Layout {
		bool has_root = false;
		std::vector<std::string> imports;
		std::string style;
	};

	void add_component(const std::string &path, std::set<std::string> &visited, std::vector<std::string> &out) const;

	std::vector<std::string> m_base;
	std::map<std::string, Layout> m_layouts;
	Uint64 m_revision = 0;
};

}

#endif
