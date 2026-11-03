--!strict

--- The math Luau's `math` and `vector` libraries don't have. Those have the rest: math.lerp, math.map (remap),
--- math.clamp, math.noise (3D Perlin), vector.lerp, vector.dot, vector.cross, vector.angle... Angles are in degrees,
--- like everywhere in the Aquila API.
local Math = {}

--- 0 below edge0, 1 above edge1, a smooth S curve between them, like GLSL's smoothstep.
function Math.smooth_step(edge0: number, edge1: number, x: number): number
	local t = math.clamp((x - edge0) / (edge1 - edge0), 0, 1)
	return t * t * (3 - 2 * t)
end

--- Where `value` sits between a and b: 0 at a, 1 at b. Not clamped, 0 when a and b are the same.
function Math.inverse_lerp(a: number, b: number, value: number): number
	if a == b then
		return 0
	end
	return (value - a) / (b - a)
end

--- Closer than `epsilon`, 1e-6 when left out.
function Math.approx_equal(a: number, b: number, epsilon: number?): boolean
	return math.abs(a - b) <= (epsilon or 1e-6)
end

--- Steps toward target by at most max_delta, never past it.
function Math.move_towards(current: number, target: number, max_delta: number): number
	local delta = target - current
	if math.abs(delta) <= max_delta then
		return target
	end
	return current + math.sign(delta) * max_delta
end

--- Eases toward target the same way at any frame rate: about 63% of the way after 1 / rate seconds. Use it instead
--- of lerp(current, target, k) in on_update, which slows down when the frame rate drops.
function Math.damp(current: number, target: number, rate: number, dt: number): number
	return current + (target - current) * (1 - math.exp(-rate * dt))
end

--- The shortest signed turn from one angle to another, in -180..180.
function Math.angle_difference(from: number, to: number): number
	local difference = (to - from) % 360
	if difference > 180 then
		difference -= 360
	end
	return difference
end

function Math.distance(a: vector, b: vector): number
	return vector.magnitude(b - a)
end

--- Steps toward target by at most max_distance, never past it.
function Math.move_towards_vector(current: vector, target: vector, max_distance: number): vector
	local offset = target - current
	local distance = vector.magnitude(offset)
	if distance <= max_distance then
		return target
	end
	return current + offset * (max_distance / distance)
end

--- Math.damp for vectors (colors too, alpha included).
function Math.damp_vector(current: vector, target: vector, rate: number, dt: number): vector
	return current + (target - current) * (1 - math.exp(-rate * dt))
end

--- `v` bounced off a surface. `normal` must be unit length.
function Math.reflect(v: vector, normal: vector): vector
	return v - normal * (2 * vector.dot(v, normal))
end

--- The part of `v` along `onto`, which can be any length but zero.
function Math.project(v: vector, onto: vector): vector
	return onto * (vector.dot(v, onto) / vector.dot(onto, onto))
end

--- `v` flattened onto the plane `normal` faces.
function Math.project_on_plane(v: vector, normal: vector): vector
	return v - Math.project(v, normal)
end

--- `v` turned around `axis` by `degrees`, the same way Entity:rotate turns an entity.
function Math.rotate_vector(v: vector, axis: vector, degrees: number): vector
	local k = vector.normalize(axis)
	local angle = math.rad(degrees)
	local c = math.cos(angle)
	return v * c + vector.cross(k, v) * math.sin(angle) + k * (vector.dot(k, v) * (1 - c))
end

--- Layered Perlin noise in -1..1: `octaves` layers of math.noise (4 when left out), each `lacunarity` (2) times
--- finer and `gain` (0.5) times weaker. One octave is math.noise.
function Math.fbm(p: vector, octaves: number?, lacunarity: number?, gain: number?): number
	local finer = lacunarity or 2
	local weaker = gain or 0.5
	local sum = 0
	local total = 0
	local amplitude = 1
	local frequency = 1
	for _ = 1, octaves or 4 do
		sum += amplitude * math.noise(p.x * frequency, p.y * frequency, p.z * frequency)
		total += amplitude
		amplitude *= weaker
		frequency *= finer
	end
	return if total > 0 then sum / total else 0
end

--- From min up to, not including, max.
function Math.random_range(min: number, max: number): number
	return min + math.random() * (max - min)
end

--- A random direction, every one as likely.
function Math.random_unit_vector(): vector
	local z = math.random() * 2 - 1
	local angle = math.random() * math.pi * 2
	local radius = math.sqrt(1 - z * z)
	return vector.create(radius * math.cos(angle), radius * math.sin(angle), z)
end

return Math
