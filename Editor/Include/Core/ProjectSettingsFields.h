#ifndef AQUILA_EDITOR_CORE_PROJECT_SETTINGS_FIELDS_H
#define AQUILA_EDITOR_CORE_PROJECT_SETTINGS_FIELDS_H

#include "Aquila/Foundation/Reflection/TypeInfo.h"
#include "Core/ProjectManager.h"

namespace Editor {

/// Shared by the Settings and Build windows. Choices are read from the project when added.
void add_startup_scene_field(Aquila::Reflection::TypeInfo &type, const ProjectInfo &project);
void add_icon_field(Aquila::Reflection::TypeInfo &type, const ProjectInfo &project);
void add_window_size_fields(Aquila::Reflection::TypeInfo &type);
void add_build_output_fields(Aquila::Reflection::TypeInfo &type);

}

#endif
