--!strict

local native = require("@game")

--- The game as a whole.
local Game = {}

--- Ends the game once this frame's scripts have run: the editor goes back to editing, a built game closes.
function Game.quit()
	native.quit()
end

return Game
