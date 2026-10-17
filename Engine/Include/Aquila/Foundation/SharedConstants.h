#ifndef AQUILA_SHARED_CONSTANTS_H
#define AQUILA_SHARED_CONSTANTS_H

#include "Aquila/Foundation/Defines.h"
#include "Aquila/Foundation/PrimitiveTypes.h"

namespace Aquila::SharedConstants {

/// Native paths baked in by CMake at configure time.
AQUILA_INLINE const std::string SHADERS_DIR = AQUILA_SHADERS_DIR;
AQUILA_INLINE const std::string RESOURCES_DIR = AQUILA_RESOURCES_DIR;

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

constexpr F64 FIXED_TIMESTEP = 1.0 / 60.0;
constexpr F64 MAX_DELTA_TIME = 0.25;

constexpr F32 WORLD_SCALE = 1.0F; // 1 unit = 1 meter

constexpr int MAX_ENTITIES = 10000;
constexpr int MAX_COMPONENTS = 64;
constexpr int INVALID_ENTITY_ID = -1;

constexpr Usize NAME_MAX_LEN = 128;
constexpr Usize PATH_MAX_LEN = 512;
constexpr int MAX_LOADED_ASSETS = 1024;

constexpr int MAX_SCENES = 32;
constexpr int MAX_SCENE_OBJECTS = 5000;

constexpr Uint32 MAX_CAMERAS = 8;
constexpr Uint32 MAX_LIGHTS = 256; // max punctual lights uploaded to GPU per frame
constexpr Uint32 MAX_MATERIALS = 4096; // max entities with MaterialComponent per frame

constexpr Uint32 CLUSTER_GRID_X = 16;
constexpr Uint32 CLUSTER_GRID_Y = 9;
constexpr Uint32 CLUSTER_GRID_Z = 24;
constexpr Uint32 CLUSTER_COUNT = CLUSTER_GRID_X * CLUSTER_GRID_Y * CLUSTER_GRID_Z; // 3456
constexpr Uint32 MAX_LIGHTS_PER_CLUSTER = 256;

constexpr int MAX_KEY_STATES = 512;
constexpr int MAX_MOUSE_STATES = 8;

constexpr Uint32 MAX_QUADS = 10000;
constexpr Uint32 VERTS_PER_QUAD = 4;
constexpr Uint32 INDICES_PER_QUAD = 6;

constexpr int FONT_FIRST_CODEPOINT = 32;
constexpr int FONT_GLYPH_COUNT = 96; // ASCII 32-127, seeded at atlas creation
constexpr Uint32 FONT_TEX_WIDTH = 4096;
constexpr Uint32 FONT_BAND_COUNT = 16;
constexpr Uint32 FONT_BAND_MAX = FONT_BAND_COUNT - 1;
constexpr Uint32 FONT_TEXELS_PER_CURVE = 2;

constexpr F32 FONT_DEFAULT_SIZE = 16.F;
constexpr F32 FONT_BAND_OVERLAP_EM = 1.F / 1024.F;
constexpr F32 FONT_AXIS_LINE_EPSILON_EM = 1.0e-5F;

// Reserved rows for the on-demand glyph atlas. Glyphs are appended into these
// pre-allocated textures so the GPU texture pointer stays stable as new
// codepoints are loaded at runtime.
constexpr Uint32 FONT_CURVE_RESERVE_ROWS = 48;
constexpr Uint32 FONT_BAND_RESERVE_ROWS = 48;

constexpr Int32 Z_MIN = -4096;
constexpr Int32 Z_MAX = 4096;
constexpr Usize Z_RANGE = Z_MAX - Z_MIN + 1; // 4096 + 4096 + 1 = 8193

}; // namespace Aquila::SharedConstants

#endif
