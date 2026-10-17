#pragma once

#include "Aquila/Foundation/Defines.h"
#include "Aquila/Foundation/Math/Math.h"

namespace Aquila::Math::Geometry::Bezier {
struct QuadraticBezier {
	Vec2 p0;
	Vec2 p1;
	Vec2 p2;
};

struct BezierCoefficients {
	Vec2 a;
	Vec2 b;
	Vec2 c;
};

struct BezierBounds {
	Vec2 min;
	Vec2 max;
};

/// When was_split is false, left and right are both the original curve.
struct BezierSplit {
	QuadraticBezier left;
	QuadraticBezier right;
	bool was_split;
};

struct BezierExtrema {
	Option<F32> t_x;
	Option<F32> t_y;
};

AQUILA_FORCE_INLINE Vec2 evaluate(const QuadraticBezier &curve, F32 t) {
	F32 u = 1.0F - t;
	return (u * u) * curve.p0 + (2.0F * u * t) * curve.p1 + (t * t) * curve.p2;
}

AQUILA_FORCE_INLINE Vec2 evaluate_first_derivative(const QuadraticBezier &curve, F32 t) {
	F32 u = 1.0F - t;
	return (2.0F * u) * (curve.p1 - curve.p0) + (2.0F * t) * (curve.p2 - curve.p1);
}

AQUILA_FORCE_INLINE Vec2 evaluate_second_derivative(const QuadraticBezier &curve) {
	return 2.0F * (curve.p2 - (2.0F * curve.p1) + curve.p0);
}

AQUILA_FORCE_INLINE BezierCoefficients compute_coefficients(const QuadraticBezier &curve) {
	// b(t) = at^2 + bt + c
	return {
		.a = curve.p0 - 2.0F * curve.p1 + curve.p2,
		.b = 2.0F * (curve.p1 - curve.p0),
		.c = curve.p0,
	};
}

// https://iquilezles.org/articles/bezierbbox/
AQUILA_FORCE_INLINE BezierBounds compute_bounds(const QuadraticBezier &curve) {
	BezierBounds bounds{};
	bounds.min = Math::min<Vec2>(curve.p0, curve.p2);
	bounds.max = Math::max<Vec2>(curve.p0, curve.p2);

	if (curve.p1.x < bounds.min.x || curve.p1.x > bounds.max.x || curve.p1.y < bounds.min.y ||
		curve.p1.y > bounds.max.y) {
		Vec2 a = curve.p0 - 2.0F * curve.p1 + curve.p2;
		Vec2 b = 2.0F * (curve.p1 - curve.p0);

		Vec2 t = Math::clamp(-b / (2.0F * a), 0.0F, 1.0F);
		Vec2 s = 1.0F - t;
		Vec2 q = s * s * curve.p0 + 2.0F * s * t * curve.p1 + t * t * curve.p2;
		bounds.min = Math::min<Vec2>(bounds.min, q);
		bounds.max = Math::max<Vec2>(bounds.max, q);
	}

	return bounds;
}

/// t where the curve turns around in x or y. Can be outside 0..1, nullopt when it never turns on that axis.
AQUILA_FORCE_INLINE BezierExtrema find_extrema(const QuadraticBezier &curve) {
	BezierCoefficients coefficients = compute_coefficients(curve);
	BezierExtrema extrema{};
	if (Math::abs(coefficients.a.x) > Math::EPSILON) {
		extrema.t_x = -coefficients.b.x / (2.0F * coefficients.a.x);
	}

	if (Math::abs(coefficients.a.y) > Math::EPSILON) {
		extrema.t_y = -coefficients.b.y / (2.0F * coefficients.a.y);
	}
	return extrema;
}

// le goat DeCasteljau
AQUILA_FORCE_INLINE BezierSplit split(const QuadraticBezier &curve, F32 t) {
	Vec2 q0 = Math::lerp(curve.p0, curve.p1, t);
	Vec2 q1 = Math::lerp(curve.p1, curve.p2, t);
	Vec2 mid = Math::lerp(q0, q1, t);
	return {
		.left = QuadraticBezier{ .p0 = curve.p0, .p1 = q0, .p2 = mid },
		.right = QuadraticBezier{ .p0 = mid, .p1 = q1, .p2 = curve.p2 },
		.was_split = true,
	};
}

/// Splits where the curve turns around in y, so both halves only go up or only go down.
/// Nothing to split when the turn isn't inside the curve.
AQUILA_FORCE_INLINE BezierSplit split_at_y_extrema(const QuadraticBezier &curve) {
	BezierExtrema extrema = find_extrema(curve);
	if (extrema.t_y.has_value()) {
		F32 t = extrema.t_y.value();
		if (t > 0.0F && t < 1.0F) {
			return split(curve, t);
		}
	}
	return { .left = curve, .right = curve, .was_split = false };
}

AQUILA_FORCE_INLINE BezierSplit split_at_x_extrema(const QuadraticBezier &curve) {
	BezierExtrema extrema = find_extrema(curve);
	if (extrema.t_x.has_value()) {
		F32 t = extrema.t_x.value();
		if (t > 0.0F && t < 1.0F) {
			return split(curve, t);
		}
	}
	return { .left = curve, .right = curve, .was_split = false };
}

} // namespace Aquila::Math::Bezier
