--!strict

local native = require("@input")

--- Keys by name: letters and digits as themselves ("W", "1"), F1 to F12 and the names below. "Shift", "Ctrl"
--- and "Alt" match either side.
export type Key =
	"A" | "B" | "C" | "D" | "E" | "F" | "G" | "H" | "I" | "J" | "K" | "L" | "M" | "N" | "O" | "P" | "Q" | "R"
	| "S" | "T" | "U" | "V" | "W" | "X" | "Y" | "Z" | "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9"
	| "F1" | "F2" | "F3" | "F4" | "F5" | "F6" | "F7" | "F8" | "F9" | "F10" | "F11" | "F12" | "Space" | "Enter"
	| "Esc" | "Tab" | "Backspace" | "Insert" | "Delete" | "Left" | "Right" | "Up" | "Down" | "PageUp"
	| "PageDown" | "Home" | "End" | "Shift" | "Ctrl" | "Alt" | "LeftShift" | "RightShift" | "LeftCtrl"
	| "RightCtrl" | "LeftAlt" | "RightAlt" | "'" | "," | "-" | "." | "/" | ";" | "=" | "[" | "\\" | "]" | "`"

export type MouseButton = "Left" | "Right" | "Middle"

--- What the game window gets. In the editor that's only while the Game view has focus (click it), so nothing
--- reads as held while you work in the editor.
local Input = {}

local either: { [string]: { string } } = {
	Shift = { "LeftShift", "RightShift" },
	Ctrl = { "LeftCtrl", "RightCtrl" },
	Alt = { "LeftAlt", "RightAlt" },
}

local function any_side(check: (string) -> boolean, key: string): boolean
	local sides = either[key]
	if sides then
		return check(sides[1]) or check(sides[2])
	end
	return check(key)
end

--- Held down right now.
function Input.is_key_down(key: Key): boolean
	return any_side(native.is_key_down, key)
end

--- Went down since the last frame. Key repeat doesn't count.
function Input.is_key_just_pressed(key: Key): boolean
	return any_side(native.is_key_just_pressed, key)
end

--- Went up since the last frame.
function Input.is_key_just_released(key: Key): boolean
	return any_side(native.is_key_just_released, key)
end

function Input.is_mouse_button_down(button: MouseButton): boolean
	return native.is_mouse_button_down(button)
end

function Input.is_mouse_button_just_pressed(button: MouseButton): boolean
	return native.is_mouse_button_just_pressed(button)
end

function Input.is_mouse_button_just_released(button: MouseButton): boolean
	return native.is_mouse_button_just_released(button)
end

--- Pixels from the top left corner of the game view, see Aquila.Screen.get_size(). Can be outside of it.
function Input.get_mouse_position(): vector
	return native.get_mouse_position()
end

--- Pixels the mouse moved since the last frame.
function Input.get_mouse_motion(): vector
	return native.get_mouse_motion()
end

--- Wheel steps since the last frame, y is up and down.
function Input.get_scroll(): vector
	return native.get_scroll()
end

--- -1 while only `negative` is held, 1 while only `positive` is, 0 otherwise.
function Input.get_axis(negative: Key, positive: Key): number
	local value = 0
	if Input.is_key_down(negative) then
		value -= 1
	end
	if Input.is_key_down(positive) then
		value += 1
	end
	return value
end

--- A direction from four keys: x from left/right, y from down/up, z is 0. Never longer than 1, so diagonals
--- aren't faster.
---
--- ```luau
--- local move = Input.get_vector("A", "D", "S", "W")
--- self:set_position(self:get_position() + vector.create(move.x, 0, move.y) * speed * dt)
--- ```
function Input.get_vector(left: Key, right: Key, down: Key, up: Key): vector
	local direction = vector.create(Input.get_axis(left, right), Input.get_axis(down, up), 0)
	local length = vector.magnitude(direction)
	if length > 1 then
		return direction / length
	end
	return direction
end

return Input
