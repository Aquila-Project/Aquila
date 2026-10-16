#pragma once
#include <ios>
#include <cstdio>
namespace Aquila::Foundation {
struct UUID {
	uint64_t high;
	uint64_t low;

	static UUID generate() {
		std::random_device rd;
		std::mt19937_64 gen(rd());
		std::uniform_int_distribution<uint64_t> dist;

		UUID uuid{};
		uuid.high = dist(gen);
		uuid.low = dist(gen);

		// enforce UUID version 4 bits
		uuid.high &= 0xFFFFFFFFFFFF0FFFULL;
		uuid.high |= 0x0000000000004000ULL;
		uuid.low &= 0x3FFFFFFFFFFFFFFFULL;
		uuid.low |= 0x8000000000000000ULL;

		return uuid;
	}

	/// Same path gives the same id. It's built on std::hash, which differs between compilers,
	/// so don't save it in files shared between platforms.
	static UUID from_filepath(const std::string &filepath) {
		std::hash<std::string> hasher;
		size_t const hash = hasher(filepath);

		std::hash<size_t> hasher2;
		size_t const hash2 = hasher2(hash ^ 0xDEADBEEF);

		UUID uuid{};
		uuid.high = static_cast<uint64_t>(hash);
		uuid.low = static_cast<uint64_t>(hash2);

		uuid.high &= 0xFFFFFFFFFFFF0FFFULL;
		uuid.high |= 0x0000000000005000ULL; // Version 5
		uuid.low &= 0x3FFFFFFFFFFFFFFFULL;
		uuid.low |= 0x8000000000000000ULL;

		return uuid;
	}

	static UUID null() { return UUID{ .high = 0, .low = 0 }; }

	std::string to_string() const {
		std::ostringstream oss;
		oss << std::hex << std::setfill('0') << std::setw(8) << static_cast<uint32_t>(high >> 32) << "-" << std::setw(4)
			<< static_cast<uint16_t>(high >> 16) << "-" << std::setw(4) << static_cast<uint16_t>(high) << "-"
			<< std::setw(4) << static_cast<uint16_t>(low >> 48) << "-" << std::setw(12)
			<< (low & 0x0000FFFFFFFFFFFFULL);
		return oss.str();
	}

	/// Doesn't validate, a bad string gives a partial or null id.
	static UUID from_string(const std::string &str) {
		UUID uuid{};
		uint32_t data1 = 0;
		uint16_t data2 = 0;
		uint16_t data3 = 0;
		uint16_t data4 = 0;
		uint64_t data5 = 0;

#ifdef AQUILA_PLATFORM_WINDOWS
		sscanf_s(str.c_str(), "%8x-%4hx-%4hx-%4hx-%12llx", &data1, &data2, &data3, &data4, &data5);
#elif defined(AQUILA_PLATFORM_LINUX)
		std::sscanf(str.c_str(), "%8x-%4hx-%4hx-%4hx-%12" SCNx64, &data1, &data2, &data3, &data4, &data5);
#endif

		uuid.high = (static_cast<uint64_t>(data1) << 32) | (static_cast<uint64_t>(data2) << 16) | data3;

		uuid.low = (static_cast<uint64_t>(data4) << 48) | (data5 & 0x0000FFFFFFFFFFFFULL);

		return uuid;
	}

	bool operator==(const UUID &other) const { return high == other.high && low == other.low; }

	bool operator!=(const UUID &other) const { return !(*this == other); }
};
} // namespace Aquila::Foundation

namespace std {
template <> struct hash<Aquila::Foundation::UUID> {
	std::size_t operator()(const Aquila::Foundation::UUID &uuid) const noexcept {
		std::size_t h1 = std::hash<uint64_t>{}(uuid.high);
		std::size_t h2 = std::hash<uint64_t>{}(uuid.low);
		return h1 ^ (h2 << 1);
	}
};
} // namespace std
