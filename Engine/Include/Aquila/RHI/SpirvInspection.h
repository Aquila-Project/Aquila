#ifndef AQUILA_RHI_SPIRV_INSPECTION_H
#define AQUILA_RHI_SPIRV_INSPECTION_H

#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Aquila::RHI {

[[nodiscard]] bool spirv_reads_member(std::span<const Uint32> spirv, std::string_view struct_name,
									  std::string_view member_name);

}

#endif
