#pragma once

#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Aquila::UI::Core {

class DockSpace;

struct DockNodeDesc {
	bool is_leaf = true;
	F32 fraction = 1.F;

	Int32 active_index = -1;
	std::vector<std::string> panel_ids;

	/// 0 is a row, 1 is a column.
	Uint8 direction = 0;
	std::vector<DockNodeDesc> children;
};

struct DockLayoutDesc {
	DockNodeDesc root;
};

class DockLayoutSerializer {
  public:
	static DockLayoutDesc capture(const DockSpace &space);

	/// Compact binary form, decode() reads it back.
	static std::vector<Uint8> encode(const DockLayoutDesc &desc);
	static Option<DockLayoutDesc> decode(const Uint8 *data, Usize size);

	static bool save_to_file(const std::string &virtual_path, const DockLayoutDesc &desc);
	static Option<DockLayoutDesc> load_from_file(const std::string &virtual_path);
};

} // namespace Aquila::UI::Core
