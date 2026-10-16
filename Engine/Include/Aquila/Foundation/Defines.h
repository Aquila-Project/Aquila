#ifndef AQUILA_DEFINES_H
#define AQUILA_DEFINES_H

#define BIT(x) (1 << (x))

#define AQUILA_NONCOPYABLE(ClassName)      \
	ClassName(const ClassName &) = delete; \
	ClassName &operator=(const ClassName &) = delete;

/// Needs a semicolon after it, AQUILA_NONCOPYABLE doesn't.
#define AQUILA_NONMOVEABLE(ClassName) \
	ClassName(ClassName &&) = delete; \
	ClassName &operator=(ClassName &&) = delete

#define AQUILA_DEFAULT_MOVE_CONSTRUCTOR(ClassName) \
	ClassName(ClassName &&) = default;             \
	ClassName &operator=((ClassName) &&) = default

#define AQUILA_DEFAULT_COPY_CONSTRUCTOR(ClassName) \
	ClassName(ClassName &) = default;              \
	ClassName &operator=((ClassName) &) = default

// NOTE (Alex) :
// Note to self, FORCE_INLINE should be reserved for functions that are small, hot,
// and called frequently in tight loops.
// Slapping it on everything actually hurts performance by bloating instruction cache.
#ifdef _MSC_VER
#define AQUILA_INLINE inline
#define AQUILA_FORCE_INLINE __forceinline
#define AQUILA_NEVER_INLINE __declspec(noinline)
#define AQUILA_RESTRICT __restrict
#elif defined(__GNUC__) || defined(__clang__)
#define AQUILA_INLINE inline
#define AQUILA_FORCE_INLINE __attribute__((always_inline)) inline
#define AQUILA_NEVER_INLINE __attribute__((noinline))
#define AQUILA_RESTRICT __restrict__
#else
#define AQUILA_FORCE_INLINE inline
#define AQUILA_NEVER_INLINE
#define AQUILA_RESTRICT
#endif

#define AQUILA_UNUSED(x) (void)(x)

#define AQUILA_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define AQUILA_OFFSETOF(type, member) offsetof(type, member)

#ifdef _MSC_VER
#define AQUILA_DEBUG_BREAK() __debugbreak()
#elif defined(__GNUC__) || defined(__clang__)
#define AQUILA_DEBUG_BREAK() __builtin_trap()
#else
#define AQUILA_DEBUG_BREAK() ((void)0)
#endif

// NOTE (Alex) :
// This is unnecessary... It was cool when C was a thing, but im using a pretty modern C++, this can be deleted, and should be
// TODO : Delete this completely
#if defined(__GNUC__) || defined(__clang__)
#define AQUILA_LIKELY(x) __builtin_expect(!!(x), 1)
#define AQUILA_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define AQUILA_LIKELY(x) (x)
#define AQUILA_UNLIKELY(x) (x)
#endif

#ifdef _MSC_VER
#define AQUILA_DEPRECATED(msg) __declspec(deprecated(msg))
#elif defined(__GNUC__) || defined(__clang__)
#define AQUILA_DEPRECATED(msg) __attribute__((deprecated(msg)))
#else
#define AQUILA_DEPRECATED(msg)
#endif

/// Goes inside an Event subclass, fills in its name and type index.
#define EVENT_CLASS_TYPE(type)                                      \
	static const char *get_static_name() {                          \
		return #type;                                               \
	}                                                               \
	[[nodiscard]] const char *get_name() const override {           \
		return get_static_name();                                   \
	}                                                               \
	[[nodiscard]] std::type_index get_type_index() const override { \
		static const std::type_index s_index(typeid(type));         \
		return s_index;                                             \
	}

#define EVENT_CLASS_CATEGORY(category)                    \
	virtual EventCategory get_category() const override { \
		return category;                                  \
	}

#endif // AQUILA_DEFINES_H
