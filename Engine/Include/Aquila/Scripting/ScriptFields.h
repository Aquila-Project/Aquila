#ifndef AQUILA_SCRIPTING_SCRIPT_FIELDS_H
#define AQUILA_SCRIPTING_SCRIPT_FIELDS_H

#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Scene/Components/ScriptComponent.h"

namespace Aquila::Scripting {

enum class ScriptFieldType : Uint8 { Number, Boolean, String, Vector, Entity };

/// One field a script declares with Aquila.Field, as the inspector shows it.
struct ScriptField {
	std::string name;
	ScriptFieldType type = ScriptFieldType::Number;
	/// Entity fields default to a null EntityReference.
	SceneManagement::Components::ScriptFieldValue default_value = 0.0;
	/// Numbers only. Infinite when the script gives no limit.
	F64 min = -std::numeric_limits<F64>::infinity();
	F64 max = std::numeric_limits<F64>::infinity();
	/// Drag speed, 0 for the inspector's default.
	F64 step = 0.0;
	bool slider = false;
	bool integer = false;
	/// Vectors only: an RGB color picker.
	bool color = false;

	bool operator==(const ScriptField &other) const = default;
};

/// While a script loads (its top level runs): the values it gets and where its declarations go. Both optional.
struct ScriptFieldContext {
	const std::map<std::string, SceneManagement::Components::ScriptFieldValue> *values = nullptr;
	std::vector<ScriptField> *declared = nullptr;
};

/// The field's value for a component: its override when it holds the declared type, else the default.
[[nodiscard]] SceneManagement::Components::ScriptFieldValue
field_value(const ScriptField &field, const SceneManagement::Components::ScriptComponent &component);

/// Runs the script's top level in a throwaway runtime over an empty scene, so its hooks never run and whatever the
/// top level does to the scene is lost, and returns the fields it declares, in order. Errors go to the log, then
/// it returns what was declared before the error. Main thread.
[[nodiscard]] std::vector<ScriptField> read_script_fields(const std::string &path);

}

#endif
