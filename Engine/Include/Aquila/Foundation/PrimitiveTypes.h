#ifndef AQUILA_TYPES_H
#define AQUILA_TYPES_H

#include "Aquila/Foundation/Defines.h"
#include "Aquila/Foundation/Math/MathTypes.h"

using Uint8 = std::uint8_t;
using Uint16 = std::uint16_t;
using Uint32 = std::uint32_t;
using Uint64 = std::uint64_t;
using Int8 = std::int8_t;
using Int16 = std::int16_t;
using Int32 = std::int32_t;
using Int64 = std::int64_t;
using F32 = float;
using F64 = double;
using Byte = std::byte;

using Usize = std::size_t;
using Isize = std::ptrdiff_t;
using Uptr = std::uintptr_t;
using Iptr = std::intptr_t;

/// A reader/writer lock: MutexLock to write, SharedLock to read.
using Mutex = std::shared_mutex;
using MutexLock = std::unique_lock<Mutex>; // exclusive write lock
using SharedLock = std::shared_lock<Mutex>; // shared read lock

template <typename T> using Ref = std::shared_ptr<T>;
template <typename T> using Unique = std::unique_ptr<T>;
template <typename T> using WeakRef = std::weak_ptr<T>;
template <typename T> using Delegate = std::function<T>;

template <typename T> using Option = std::optional<T>;
template <typename... Ts> using Variant = std::variant<Ts...>;
/// Just a variant, read it with std::get_if or std::visit.
template <typename Ok, typename Err> using Result = std::variant<Ok, Err>;

/// One lambda per alternative for std::visit.
///
/// @code
/// std::visit(Overloaded{ [](Int32 value) { ... }, [](F32 value) { ... } }, property_value);
/// @endcode
template <typename... Visitors> struct Overloaded : Visitors... {
	using Visitors::operator()...;
};
template <typename... Visitors> Overloaded(Visitors...) -> Overloaded<Visitors...>;

enum class AccessMode : Uint8 { Read = 0, Write = 1, ReadWrite = 2 };
enum class OpenMode : Uint8 {
	Text = 0, // default, text mode
	Binary = BIT(1), // "b" no newline translation
	Append = BIT(2), // "a" writes go to end
	Truncate = BIT(3), // "w" clears file on open
	Create = BIT(4), // create if not exists
};

AQUILA_FORCE_INLINE OpenMode operator|(OpenMode a, OpenMode b) {
	return static_cast<OpenMode>(static_cast<Uint8>(a) | static_cast<Uint8>(b));
}

AQUILA_FORCE_INLINE OpenMode operator&(OpenMode a, OpenMode b) {
	return static_cast<OpenMode>(static_cast<Uint8>(a) & static_cast<Uint8>(b));
}

AQUILA_FORCE_INLINE bool has_flag(OpenMode mode, OpenMode flag) {
	return (static_cast<Uint8>(mode) & static_cast<Uint8>(flag)) != 0;
}

enum class Priority : Uint8 { VeryHigh = BIT(1), High = BIT(2), Medium = BIT(3), Low = BIT(4) };

/// Converts to the default of whatever type it's assigned to.
///
/// @code
/// Vec3 offset = DEFAULT_VALUE;
/// @endcode
struct DefaultValue {
	template <typename T> constexpr operator T() const noexcept { return T{}; }
};

AQUILA_INLINE constexpr DefaultValue DEFAULT_VALUE;

#endif
