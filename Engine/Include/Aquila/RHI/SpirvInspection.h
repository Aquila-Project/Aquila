#ifndef AQUILA_RHI_SPIRV_INSPECTION_H
#define AQUILA_RHI_SPIRV_INSPECTION_H

#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Aquila::RHI {

/// True when the shader reads that member of that struct. Works off SPIR-V debug names,
/// `struct_name` matches any piece of a name like `Aquila.Shading.FrameData_std140`.
/// Stripped SPIR-V always gives false.
///
/// @code
/// bool animated = RHI::spirv_reads_member(spirv, "FrameData", "time");
/// @endcode
[[nodiscard]] bool spirv_reads_member(std::span<const Uint32> spirv, std::string_view struct_name,
									  std::string_view member_name);

}

#endif
