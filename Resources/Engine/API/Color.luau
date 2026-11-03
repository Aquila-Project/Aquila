--!strict

--- Colors are vectors: x, y, z are red, green, blue and w is alpha. luau-lsp's vector type only knows x, y and z
--- and its vector.create takes three numbers, so make colors and read their alpha here. vector.create(r, g, b)
--- works too, but its alpha is 0.
local Color = {}

local create = vector.create :: any

--- A color, opaque when `a` is left out.
function Color.new(r: number, g: number, b: number, a: number?): vector
	return create(r, g, b, if a == nil then 1 else a)
end

--- The alpha of a color.
function Color.alpha(color: vector): number
	return (color :: any).w
end

return Color
