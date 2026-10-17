#include "Aquila/RHI/SpirvInspection.h"

namespace Aquila::RHI {

namespace {

constexpr Uint32 k_magic = 0x07230203;
constexpr Usize k_header_words = 5;

enum Opcode : Uint32 {
	OpName = 5,
	OpMemberName = 6,
	OpTypePointer = 32,
	OpConstant = 43,
	OpFunctionParameter = 55,
	OpVariable = 59,
	OpLoad = 61,
	OpAccessChain = 65,
	OpInBoundsAccessChain = 66,
	OpCompositeExtract = 81,
	OpCopyObject = 83,
};

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
	constexpr std::string_view k_separators = "._:";
	Usize start = 0;
	while (true) {
		const Usize end = name.find_first_of(k_separators, start);
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

}

bool spirv_reads_member(std::span<const Uint32> spirv, std::string_view struct_name, std::string_view member_name) {
	if (spirv.size() < k_header_words || spirv[0] != k_magic) {
		return false;
	}

	MemberReadScan scan;
	scan.struct_name = struct_name;
	scan.member_name = member_name;
	for (Usize i = k_header_words; i < spirv.size();) {
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

}
