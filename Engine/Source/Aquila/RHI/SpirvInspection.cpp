#include "Aquila/RHI/SpirvInspection.h"

namespace Aquila::RHI {

namespace {

constexpr Uint32 K_MAGIC = 0x07230203;
constexpr Usize K_HEADER_WORDS = 5;

enum Opcode : Uint32 {
	OpName = 5,
	OpMemberName = 6,
	OpEntryPoint = 15,
	OpTypeInt = 21,
	OpTypeFloat = 22,
	OpTypeVector = 23,
	OpTypeMatrix = 24,
	OpTypeArray = 28,
	OpTypeStruct = 30,
	OpTypePointer = 32,
	OpConstant = 43,
	OpFunctionParameter = 55,
	OpVariable = 59,
	OpLoad = 61,
	OpAccessChain = 65,
	OpInBoundsAccessChain = 66,
	OpDecorate = 71,
	OpCompositeExtract = 81,
	OpCopyObject = 83,
};

constexpr Uint32 K_EXECUTION_MODEL_VERTEX = 0;
constexpr Uint32 K_STORAGE_CLASS_INPUT = 1;
constexpr Uint32 K_DECORATION_LOCATION = 30;
constexpr Uint32 K_MAX_INPUT_LOCATIONS = 256;
constexpr Uint32 K_TOO_MANY_LOCATIONS = K_MAX_INPUT_LOCATIONS + 1;
constexpr Uint32 K_MAX_TYPE_DEPTH = 8;

std::string_view literal_string(std::span<const Uint32> words) {
	const auto *chars = reinterpret_cast<const char *>(words.data());
	const Usize capacity = words.size() * sizeof(Uint32);
	Usize length = 0;
	while (length < capacity && chars[length] != '\0') {
		++length;
	}
	return { chars, length };
}

bool names_struct(std::string_view name, std::string_view struct_name) {
	constexpr std::string_view K_SEPARATORS = "._:";
	Usize start = 0;
	while (true) {
		const Usize end = name.find_first_of(K_SEPARATORS, start);
		if (name.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start) == struct_name) {
			return true;
		}
		if (end == std::string_view::npos) {
			return false;
		}
		start = end + 1;
	}
}

struct MemberReadScan {
	std::string_view struct_name;
	std::string_view member_name;
	std::unordered_set<Uint32> named_structs;
	std::unordered_map<Uint32, Uint32> member_of_struct;
	std::unordered_map<Uint32, Uint32> pointee_of_pointer;
	std::unordered_map<Uint32, Uint32> constants;
	std::unordered_map<Uint32, Uint32> struct_pointers;
	std::unordered_map<Uint32, Uint32> struct_values;

	void track_pointer(Uint32 pointer_type, Uint32 id) {
		const auto pointee = pointee_of_pointer.find(pointer_type);
		if (pointee == pointee_of_pointer.end() || !named_structs.contains(pointee->second)) {
			return;
		}
		const auto member = member_of_struct.find(pointee->second);
		if (member != member_of_struct.end()) {
			struct_pointers[id] = member->second;
		}
	}

	bool reads_member(Uint32 opcode, std::span<const Uint32> ops) {
		switch (opcode) {
		case OpName:
			if (ops.size() >= 2 && names_struct(literal_string(ops.subspan(1)), struct_name)) {
				named_structs.insert(ops[0]);
			}
			return false;
		case OpMemberName:
			if (ops.size() >= 3 && literal_string(ops.subspan(2)) == member_name) {
				member_of_struct[ops[0]] = ops[1];
			}
			return false;
		case OpTypePointer:
			if (ops.size() >= 3) {
				pointee_of_pointer[ops[0]] = ops[2];
			}
			return false;
		case OpConstant:
			if (ops.size() >= 3) {
				constants[ops[1]] = ops[2];
			}
			return false;
		case OpVariable:
		case OpFunctionParameter:
			if (ops.size() >= 2) {
				track_pointer(ops[0], ops[1]);
			}
			return false;
		case OpLoad:
			if (ops.size() >= 3) {
				if (const auto it = struct_pointers.find(ops[2]); it != struct_pointers.end()) {
					struct_values[ops[1]] = it->second;
				}
			}
			return false;
		case OpCopyObject:
			if (ops.size() >= 3) {
				if (const auto it = struct_pointers.find(ops[2]); it != struct_pointers.end()) {
					struct_pointers[ops[1]] = it->second;
				}
				if (const auto it = struct_values.find(ops[2]); it != struct_values.end()) {
					struct_values[ops[1]] = it->second;
				}
			}
			return false;
		case OpAccessChain:
		case OpInBoundsAccessChain:
			if (ops.size() >= 4) {
				if (const auto it = struct_pointers.find(ops[2]); it != struct_pointers.end()) {
					const auto index = constants.find(ops[3]);
					if (index != constants.end() && index->second == it->second) {
						return true;
					}
				}
				track_pointer(ops[0], ops[1]);
			}
			return false;
		case OpCompositeExtract:
			if (ops.size() >= 4) {
				const auto it = struct_values.find(ops[2]);
				return it != struct_values.end() && ops[3] == it->second;
			}
			return false;
		default:
			return false;
		}
	}
};

struct VertexInputScan {
	struct Type {
		Uint32 opcode = 0;
		Uint32 element = 0;
		Uint32 count = 0;
	};

	std::string_view entry_point;
	bool found = false;
	std::vector<Uint32> interface;
	std::unordered_map<Uint32, Uint32> location_of;
	std::unordered_map<Uint32, Uint32> input_pointee_of_pointer;
	std::unordered_map<Uint32, Uint32> input_variables;
	std::unordered_map<Uint32, Uint32> constants;
	std::unordered_map<Uint32, Type> types;
	std::unordered_map<Uint32, std::vector<Uint32>> struct_members;

	void read(Uint32 opcode, std::span<const Uint32> ops) {
		switch (opcode) {
		case OpEntryPoint:
			if (!found && ops.size() >= 3 && ops[0] == K_EXECUTION_MODEL_VERTEX) {
				const std::string_view name = literal_string(ops.subspan(2));
				if (name == entry_point) {
					found = true;
					const Usize first_interface = 2 + (name.size() / sizeof(Uint32)) + 1;
					if (first_interface < ops.size()) {
						interface.assign(ops.begin() + static_cast<std::ptrdiff_t>(first_interface), ops.end());
					}
				}
			}
			return;
		case OpDecorate:
			if (ops.size() >= 3 && ops[1] == K_DECORATION_LOCATION) {
				location_of[ops[0]] = ops[2];
			}
			return;
		case OpTypeInt:
		case OpTypeFloat:
			if (ops.size() >= 2) {
				types[ops[0]] = { .opcode = opcode, .element = ops[1] };
			}
			return;
		case OpTypeVector:
		case OpTypeMatrix:
		case OpTypeArray:
			if (ops.size() >= 3) {
				types[ops[0]] = { .opcode = opcode, .element = ops[1], .count = ops[2] };
			}
			return;
		case OpTypeStruct:
			if (!ops.empty()) {
				types[ops[0]] = { .opcode = opcode };
				struct_members[ops[0]].assign(ops.begin() + 1, ops.end());
			}
			return;
		case OpTypePointer:
			if (ops.size() >= 3 && ops[1] == K_STORAGE_CLASS_INPUT) {
				input_pointee_of_pointer[ops[0]] = ops[2];
			}
			return;
		case OpConstant:
			if (ops.size() >= 3) {
				constants[ops[1]] = ops[2];
			}
			return;
		case OpVariable:
			if (ops.size() >= 3 && ops[2] == K_STORAGE_CLASS_INPUT) {
				if (const auto it = input_pointee_of_pointer.find(ops[0]); it != input_pointee_of_pointer.end()) {
					input_variables[ops[1]] = it->second;
				}
			}
			return;
		default:
			return;
		}
	}

	[[nodiscard]] Uint32 scalar_width(Uint32 type) const {
		const auto it = types.find(type);
		return it == types.end() ? 32 : it->second.element;
	}

	[[nodiscard]] Uint32 location_count(Uint32 type, Uint32 depth = 0) const {
		const auto it = types.find(type);
		if (it == types.end() || depth > K_MAX_TYPE_DEPTH) {
			return 1;
		}
		const Type &info = it->second;
		Uint32 count = 1;
		switch (info.opcode) {
		case OpTypeVector:
			count = scalar_width(info.element) == 64 && info.count > 2 ? 2 : 1;
			break;
		case OpTypeMatrix:
			count = std::min(info.count, K_TOO_MANY_LOCATIONS) * location_count(info.element, depth + 1);
			break;
		case OpTypeArray: {
			const auto length = constants.find(info.count);
			const Uint32 elements = length == constants.end() ? 1 : length->second;
			count = std::min(elements, K_TOO_MANY_LOCATIONS) * location_count(info.element, depth + 1);
			break;
		}
		case OpTypeStruct:
			count = 0;
			for (const Uint32 member : struct_members.at(type)) {
				count = std::min(count + location_count(member, depth + 1), K_TOO_MANY_LOCATIONS);
			}
			break;
		default:
			break;
		}
		return std::min(count, K_TOO_MANY_LOCATIONS);
	}

	[[nodiscard]] Option<std::vector<Uint32>> locations() const {
		if (!found) {
			return std::nullopt;
		}
		std::vector<Uint32> out;
		for (const Uint32 id : interface) {
			const auto variable = input_variables.find(id);
			const auto location = location_of.find(id);
			if (variable == input_variables.end() || location == location_of.end()) {
				continue;
			}
			const Uint32 count = location_count(variable->second);
			if (count > K_MAX_INPUT_LOCATIONS || location->second > K_MAX_INPUT_LOCATIONS) {
				return std::nullopt;
			}
			for (Uint32 i = 0; i < count; ++i) {
				out.push_back(location->second + i);
			}
		}
		std::ranges::sort(out);
		const auto duplicates = std::ranges::unique(out);
		out.erase(duplicates.begin(), duplicates.end());
		return out;
	}
};
}

bool spirv_reads_member(std::span<const Uint32> spirv, std::string_view struct_name, std::string_view member_name) {
	if (spirv.size() < K_HEADER_WORDS || spirv[0] != K_MAGIC) {
		return false;
	}

	MemberReadScan scan;
	scan.struct_name = struct_name;
	scan.member_name = member_name;
	for (Usize i = K_HEADER_WORDS; i < spirv.size();) {
		const Uint32 word_count = spirv[i] >> 16;
		if (word_count == 0 || i + word_count > spirv.size()) {
			return false;
		}
		if (scan.reads_member(spirv[i] & 0xFFFFU, spirv.subspan(i + 1, word_count - 1))) {
			return true;
		}
		i += word_count;
	}
	return false;
}

Option<std::vector<Uint32>> spirv_vertex_input_locations(std::span<const Uint32> spirv, std::string_view entry_point) {
	if (spirv.size() < K_HEADER_WORDS || spirv[0] != K_MAGIC) {
		return std::nullopt;
	}

	VertexInputScan scan;
	scan.entry_point = entry_point;
	for (Usize i = K_HEADER_WORDS; i < spirv.size();) {
		const Uint32 word_count = spirv[i] >> 16;
		if (word_count == 0 || i + word_count > spirv.size()) {
			return std::nullopt;
		}
		scan.read(spirv[i] & 0xFFFFU, spirv.subspan(i + 1, word_count - 1));
		i += word_count;
	}
	return scan.locations();
}
}
