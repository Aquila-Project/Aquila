#ifndef LIGHT_COMPONENT_H
#define LIGHT_COMPONENT_H

#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Foundation/Macros.h"
#include "Aquila/Foundation/Photometry.h"
#include "Aquila/Foundation/Signal.h"

namespace Aquila::SceneManagement::Components {

struct ShadowQualitySettings {
	F32 light_size = 8.0F;
	F32 shadow_bias = 0.0035F;
	F32 normal_bias = 1.0F;
	int pcf_samples = 9;
	F32 cascade_split_lambda = 0.95F;
	int blocker_search_samples = 16;
};

/// `m_intensity` is lux for Directional and lumens for Point and Spot (Foundation/Photometry.h).
/// `m_range` is for Point and Spot. `m_direction` is for Directional and Spot, it's not the
/// transform rotation, and a zero vector is ignored. Cone angles are half angles in degrees.
/// Area has no shading yet.
struct LightComponent {
	enum class Type { Point, Directional, Spot, Area };

	/// A clear noon sun.
	static constexpr F32 DEFAULT_ILLUMINANCE = 100000.0F;
	/// A 60 W incandescent bulb.
	static constexpr F32 DEFAULT_POWER = 800.0F;

	[[nodiscard]] static constexpr F32 default_intensity(Type type) noexcept {
		return type == Type::Directional ? DEFAULT_ILLUMINANCE : DEFAULT_POWER;
	}

	Type m_type = Type::Directional;

	Vec3 m_color = Vec3(1.0F);
	F32 m_intensity = DEFAULT_ILLUMINANCE;
	F32 m_range = 100.0F;

	F32 m_inner_cone_angle = 0.0F;
	F32 m_outer_cone_angle = 45.0F;

	Vec3 m_direction = Vec3(0.0F, -1.0F, 0.0F);

	Vec2 m_area_size = Vec2(1.0F, 1.0F);

	ShadowQualitySettings m_shadow_settings;

	bool m_is_active = true;

  public:
	Signal<void()> on_changed;

	bool is_active() const { return m_is_active; }
	void set_active(bool active) { m_is_active = active; }

	Type get_type() const noexcept { return m_type; }
	void set_type(Type type) noexcept { m_type = type; }

	const Vec3 &get_color() const noexcept { return m_color; }
	void set_color(const Vec3 &color) noexcept { m_color = color; }

	F32 get_intensity() const noexcept { return m_intensity; }
	void set_intensity(F32 intensity) noexcept { m_intensity = intensity; }

	F32 get_range() const noexcept { return m_range; }
	void set_range(F32 range) noexcept { m_range = range; }

	F32 get_inner_cone_angle() const noexcept { return m_inner_cone_angle; }
	void set_inner_cone_angle(F32 angle) noexcept { m_inner_cone_angle = angle; }

	F32 get_outer_cone_angle() const noexcept { return m_outer_cone_angle; }
	void set_outer_cone_angle(F32 angle) noexcept { m_outer_cone_angle = angle; }

	const Vec3 &get_direction() const noexcept { return m_direction; }
	void set_direction(const Vec3 &direction) noexcept { m_direction = direction; }

	ShadowQualitySettings &get_shadow_settings() noexcept { return m_shadow_settings; }
	const ShadowQualitySettings &get_shadow_settings() const noexcept { return m_shadow_settings; }
	void set_shadow_settings(const ShadowQualitySettings &settings) noexcept { m_shadow_settings = settings; }

	LightComponent() = default;

	LightComponent(Type light_type, const Vec3 &light_color, F32 light_intensity)
		: m_type(light_type), m_color(light_color), m_intensity(light_intensity) {}
};
} // namespace Aquila::SceneManagement::Components
#endif
