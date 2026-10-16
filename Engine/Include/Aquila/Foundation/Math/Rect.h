#pragma once

#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Foundation/Macros.h"

struct Rect {
	Vec2 position{ 0.F, 0.F }; // top-left
	Vec2 size{ 0.F, 0.F };

	[[nodiscard]] AQUILA_FORCE_INLINE float left() const { return position.x; }
	[[nodiscard]] AQUILA_FORCE_INLINE float top() const { return position.y; }
	[[nodiscard]] AQUILA_FORCE_INLINE float right() const { return position.x + size.x; }
	[[nodiscard]] AQUILA_FORCE_INLINE float bottom() const { return position.y + size.y; }
	[[nodiscard]] AQUILA_FORCE_INLINE float width() const { return size.x; }
	[[nodiscard]] AQUILA_FORCE_INLINE float height() const { return size.y; }
	[[nodiscard]] AQUILA_FORCE_INLINE Vec2 center() const { return position + size * 0.5F; }

	/// The right and bottom edges are outside.
	[[nodiscard]] AQUILA_FORCE_INLINE bool contains(Vec2 p) const {
		return p.x >= left() && p.x < right() && p.y >= top() && p.y < bottom();
	}

	/// Rects that only touch count as overlapping.
	[[nodiscard]] AQUILA_FORCE_INLINE bool overlaps(const Rect &other) const {
		return left() <= other.right() && right() >= other.left() && top() <= other.bottom() && bottom() >= other.top();
	}

	/// Empty rects aren't skipped, an empty rect at the origin still stretches the result to the origin.
	[[nodiscard]] AQUILA_FORCE_INLINE Rect Union(const Rect &other) const {
		const float x = Aquila::Math::min(left(), other.left());
		const float y = Aquila::Math::min(top(), other.top());
		const float r = Aquila::Math::max(right(), other.right());
		const float b = Aquila::Math::max(bottom(), other.bottom());
		return { .position = { x, y }, .size = { r - x, b - y } };
	}

	/// Zero size when they don't overlap.
	[[nodiscard]] AQUILA_FORCE_INLINE Rect intersect(const Rect &other) const {
		float l = Aquila::Math::max(left(), other.left());
		float t = Aquila::Math::max(top(), other.top());
		float r = Aquila::Math::min(right(), other.right());
		float b = Aquila::Math::min(bottom(), other.bottom());
		return { .position = { l, t }, .size = { Aquila::Math::max(0.F, r - l), Aquila::Math::max(0.F, b - t) } };
	}

	static AQUILA_FORCE_INLINE Rect from_min_max(Vec2 min, Vec2 max) { return { .position = min, .size = max - min }; }

	[[nodiscard]] AQUILA_FORCE_INLINE bool is_empty() const { return size.x <= 0.F || size.y <= 0.F; }

	[[nodiscard]] AQUILA_FORCE_INLINE bool operator==(const Rect &other) const {
		return position == other.position && size == other.size;
	}
	[[nodiscard]] AQUILA_FORCE_INLINE bool operator!=(const Rect &other) const { return !(*this == other); }
};
