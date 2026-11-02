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

/// Sorted locations the vertex entry point takes as inputs. Slang drops the inputs a shader never reads,
/// so this is what a pipeline's vertex attributes have to match. A matrix or array covers one location
/// per column or element.
/// @return nullopt when the SPIR-V can't be read or has no vertex entry point with that name
[[nodiscard]] Option<std::vector<Uint32>> spirv_vertex_input_locations(std::span<const Uint32> spirv,
																	   std::string_view entry_point);
}

#endif
