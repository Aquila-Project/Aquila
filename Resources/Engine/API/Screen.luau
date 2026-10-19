--!strict

local native = require("@screen")

--- Where the game is shown: the window in a built game, the Game view in the editor.
local Screen = {}

--- Pixels, x is the width and y the height. Zero until the game is on screen.
function Screen.get_size(): vector
	return native.get_size()
end

return Screen
