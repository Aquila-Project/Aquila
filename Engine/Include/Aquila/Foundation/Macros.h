#ifndef AQUILA_MACROS_H
#define AQUILA_MACROS_H

#include "Aquila/Foundation/Log.h"

#define AQUILA_LOG_TRACE(...) Aquila::Foundation::log_trace(__VA_ARGS__)
#define AQUILA_LOG_DEBUG(...) Aquila::Foundation::log_debug(__VA_ARGS__)
#define AQUILA_LOG_INFO(...) Aquila::Foundation::log_info(__VA_ARGS__)
#define AQUILA_LOG_WARNING(...) Aquila::Foundation::log_warning(__VA_ARGS__)
#define AQUILA_LOG_ERROR(...) Aquila::Foundation::log_error(__VA_ARGS__)
#define AQUILA_LOG_CRITICAL(...) Aquila::Foundation::log_critical(__VA_ARGS__)

#ifdef AQUILA_DEBUG
/// Checked in release builds too.
#define AQUILA_ASSERT(condition, message)                                              \
	do {                                                                               \
		if (AQUILA_UNLIKELY(!(condition))) {                                           \
			Aquila::Foundation::assert_failed(#condition, message, __FILE__, __LINE__); \
		}                                                                              \
	} while (0)

#else
#define AQUILA_ASSERT(condition, message)                                              \
	do {                                                                               \
		if (!(condition)) {                                                            \
			Aquila::Foundation::assert_failed(#condition, message, __FILE__, __LINE__); \
		}                                                                              \
	} while (false)
#endif

#define AQUILA_VULKAN_CHECK(call)                                                                                 \
	do {                                                                                                          \
		VkResult result = (call);                                                                                 \
		if (AQUILA_UNLIKELY(result != VK_SUCCESS)) {                                                              \
			Aquila::Foundation::log_error(std::string("[VULKAN] Call failed: ") + #call + " | File: " + __FILE__ + \
										 " | Line: " + std::to_string(__LINE__));                                 \
			Aquila::Foundation::assert_failed(#call, "Vulkan call failed", __FILE__, __LINE__);                    \
		}                                                                                                         \
	} while (0)

/// Logs a warning and keeps going.
#define AQUILA_UNIMPLEMENTED() AQUILA_LOG_WARNING("Not implemented: {}", __FUNCTION__)

#endif
