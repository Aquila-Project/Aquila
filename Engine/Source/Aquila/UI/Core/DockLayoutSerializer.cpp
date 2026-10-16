#include "Aquila/UI/Core/DockLayoutSerializer.h"

#include "Aquila/Foundation/Math/Rect.h"
#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"
#include "Aquila/UI/Style/StyleProperties.h"
#include "Aquila/UI/Style/StyleTypes.h"
#include "Aquila/UI/Widgets/DockNode.h"
#include "Aquila/UI/Widgets/DockPanel.h"
#include "Aquila/UI/Widgets/DockSpace.h"

#include <bit>

namespace Aquila::UI::Core {

namespace {

constexpr std::array<Uint8, 4> k_magic = { 'A', 'Q', 'D', 'L' };
constexpr Uint32 k_version = 1;

void put_u8(std::vector<Uint8> &out, Uint8 value) {
	out.push_back(value);
}

void put_u32(std::vector<Uint8> &out, Uint32 value) {
	out.push_back(static_cast<Uint8>(value & 0xFFu));
	out.push_back(static_cast<Uint8>((value >> 8) & 0xFFu));
	out.push_back(static_cast<Uint8>((value >> 16) & 0xFFu));
	out.push_back(static_cast<Uint8>((value >> 24) & 0xFFu));
}

void put_i32(std::vector<Uint8> &out, Int32 value) {
	put_u32(out, static_cast<Uint32>(value));
}

void put_f32(std::vector<Uint8> &out, F32 value) {
	put_u32(out, std::bit_cast<Uint32>(value));
}

void put_string(std::vector<Uint8> &out, const std::string &value) {
	put_u32(out, static_cast<Uint32>(value.size()));
	out.insert(out.end(), value.begin(), value.end());
}

void encode_node(std::vector<Uint8> &out, const DockNodeDesc &node) {
	put_u8(out, node.is_leaf ? 1u : 0u);
	put_f32(out, node.fraction);

	if (node.is_leaf) {
		put_i32(out, node.active_index);
		put_u32(out, static_cast<Uint32>(node.panel_ids.size()));
		for (const auto &id : node.panel_ids) {
			put_string(out, id);
		}
		return;
	}

	put_u8(out, node.direction);
	put_u32(out, static_cast<Uint32>(node.children.size()));
	for (const auto &child : node.children) {
		encode_node(out, child);
	}
}

struct Reader {
	const Uint8 *data = nullptr;
	Usize size = 0;
	Usize pos = 0;
	bool ok = true;

	bool ensure(Usize count) {
		if (!ok || pos + count > size) {
			ok = false;
			return false;
		}
		return true;
	}

	Uint8 u8() {
		if (!ensure(1)) {
			return 0;
		}
		return data[pos++];
	}

	Uint32 u32() {
		if (!ensure(4)) {
			return 0;
		}
		const Uint32 value = static_cast<Uint32>(data[pos]) | (static_cast<Uint32>(data[pos + 1]) << 8) |
							 (static_cast<Uint32>(data[pos + 2]) << 16) | (static_cast<Uint32>(data[pos + 3]) << 24);
		pos += 4;
		return value;
	}

	Int32 i32() {
		return static_cast<Int32>(u32());
	}

	F32 f32() {
		return std::bit_cast<F32>(u32());
	}

	std::string str() {
		const Uint32 length = u32();
		if (!ensure(length)) {
			return {};
		}
		std::string value(reinterpret_cast<const char *>(data + pos), length);
		pos += length;
		return value;
	}
};

bool decode_node(Reader &reader, DockNodeDesc &node) {
	node.is_leaf = (reader.u8() != 0);
	node.fraction = reader.f32();
	if (!reader.ok) {
		return false;
	}

	if (node.is_leaf) {
		node.active_index = reader.i32();
		const Uint32 count = reader.u32();
		if (!reader.ok || count > reader.size - reader.pos) {
			return false;
		}
		node.panel_ids.reserve(count);
		for (Uint32 i = 0; i < count; ++i) {
			node.panel_ids.push_back(reader.str());
			if (!reader.ok) {
				return false;
			}
		}
		return true;
	}

	node.direction = reader.u8();
	const Uint32 count = reader.u32();
	if (!reader.ok || count > reader.size - reader.pos) {
		return false;
	}
	node.children.resize(count);
	for (Uint32 i = 0; i < count; ++i) {
		if (!decode_node(reader, node.children[i])) {
			return false;
		}
	}
	return reader.ok;
}

DockNodeDesc capture_node(const DockNode *node) {
	DockNodeDesc desc;
	if (node == nullptr) {
		return desc;
	}

	if (node->is_leaf()) {
		desc.is_leaf = true;
		desc.active_index = node->get_active_panel();
		for (const DockPanel *panel : node->get_ordered_panels()) {
			desc.panel_ids.push_back(panel != nullptr ? panel->get_id() : std::string{});
		}
		return desc;
	}

	desc.is_leaf = false;

	const StyleProperties &style = node->get_style();
	const bool vertical = style.flex_direction.has_value() && *style.flex_direction == FlexDirection::Column;
	desc.direction = vertical ? 1u : 0u;

	std::vector<const DockNode *> child_nodes;
	for (const auto &child : node->get_children()) {
		if (const DockNode *dn = view_cast<DockNode>(child.get())) {
			child_nodes.push_back(dn);
		}
	}

	std::vector<F32> sizes;
	sizes.reserve(child_nodes.size());
	F32 total = 0.F;
	for (const DockNode *child : child_nodes) {
		const Rect rect = child->get_layout_rect();
		const F32 extent = vertical ? rect.size.y : rect.size.x;
		sizes.push_back(extent);
		total += extent;
	}

	for (Usize i = 0; i < child_nodes.size(); ++i) {
		DockNodeDesc child_desc = capture_node(child_nodes[i]);
		child_desc.fraction =
			(total > 0.F) ? (sizes[i] / total) : (1.F / static_cast<F32>(child_nodes.size()));
		desc.children.push_back(std::move(child_desc));
	}

	return desc;
}

} // namespace

DockLayoutDesc DockLayoutSerializer::capture(const DockSpace &space) {
	DockLayoutDesc desc;
	desc.root = capture_node(space.get_root_node());
	desc.root.fraction = 1.F;
	return desc;
}

std::vector<Uint8> DockLayoutSerializer::encode(const DockLayoutDesc &desc) {
	std::vector<Uint8> out;
	out.insert(out.end(), k_magic.begin(), k_magic.end());
	put_u32(out, k_version);
	encode_node(out, desc.root);
	return out;
}

Option<DockLayoutDesc> DockLayoutSerializer::decode(const Uint8 *data, Usize size) {
	Reader reader{ data, size };
	if (!reader.ensure(k_magic.size() + 4)) {
		return std::nullopt;
	}
	for (const Uint8 expected : k_magic) {
		if (reader.u8() != expected) {
			return std::nullopt;
		}
	}
	if (reader.u32() != k_version) {
		return std::nullopt;
	}

	DockLayoutDesc desc;
	if (!decode_node(reader, desc.root)) {
		return std::nullopt;
	}
	return desc;
}

bool DockLayoutSerializer::save_to_file(const std::string &virtual_path, const DockLayoutDesc &desc) {
	const std::vector<Uint8> bytes = encode(desc);
	auto file = Platform::Filesystem::VirtualFileSystem::get()->open_file(virtual_path, AccessMode::Write,
																		  OpenMode::Binary);
	if (file == nullptr || !file->is_valid()) {
		return false;
	}
	file->write(bytes.data(), bytes.size());
	file->close();
	return true;
}

Option<DockLayoutDesc> DockLayoutSerializer::load_from_file(const std::string &virtual_path) {
	auto vfs = Platform::Filesystem::VirtualFileSystem::get();
	if (!vfs->exists(virtual_path)) {
		return std::nullopt;
	}

	auto file = vfs->open_file(virtual_path, AccessMode::Read, OpenMode::Binary);
	if (file == nullptr || !file->is_valid()) {
		return std::nullopt;
	}

	const Int64 file_size = file->size();
	if (file_size <= 0) {
		file->close();
		return std::nullopt;
	}

	std::vector<Uint8> buffer(static_cast<Usize>(file_size));
	file->read(buffer.data(), buffer.size());
	file->close();

	return decode(buffer.data(), buffer.size());
}

} // namespace Aquila::UI::Core
