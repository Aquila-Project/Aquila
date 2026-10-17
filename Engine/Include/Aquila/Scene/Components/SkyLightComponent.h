#ifndef SKYLIGHT_COMPONENT_H
#define SKYLIGHT_COMPONENT_H

#include "Aquila/Foundation/Math/Math.h"
#include "Aquila/Foundation/PrimitiveTypes.h"
#include "Aquila/Foundation/Macros.h"
#include "Aquila/GFX/GfxTexture.h"

namespace Aquila::SceneManagement::Components {

struct SHCoefficients {
	std::array<Vec3, 9> coeffs{};

	SHCoefficients() {
		for (auto &coefficient : coeffs) {
			coefficient = Vec3(0.0F);
		}
	}
};

enum class SkySource {
	Procedural,
	HdrImage,
};

/// Procedural sky from the sun angles, or an HDR image. Sun angles are in degrees.
/// Anything that changes the lighting marks it dirty so the irradiance gets baked again,
/// set_irradiance() marks it clean.
struct SkyLightComponent {
	Ref<GFX::GfxTexture> m_hdr_texture;
	SHCoefficients m_irradiance;

	SkySource m_source = SkySource::Procedural;

	F32 m_sun_elevation = 45.0F;
	F32 m_sun_azimuth = 180.0F;
	F32 m_turbidity = 3.0F;
	Vec3 m_ground_albedo = Vec3(0.3F);

	F32 m_intensity = 1.0F;
	F32 m_skybox_lod = 0;
	Vec3 m_tint = Vec3(1.0F);
	bool m_is_active = true;
	bool m_is_dirty = true;
	bool m_render_skybox = true;

  public:
	[[nodiscard]] bool is_active() const noexcept { return m_is_active; }
	[[nodiscard]] SkySource get_source() const noexcept { return m_source; }
	[[nodiscard]] F32 get_sun_elevation() const noexcept { return m_sun_elevation; }
	[[nodiscard]] F32 get_sun_azimuth() const noexcept { return m_sun_azimuth; }
	[[nodiscard]] F32 get_turbidity() const noexcept { return m_turbidity; }
	[[nodiscard]] Vec3 get_ground_albedo() const noexcept { return m_ground_albedo; }
	[[nodiscard]] F32 get_intensity() const noexcept { return m_intensity; }
	[[nodiscard]] F32 get_skybox_lod() const noexcept { return m_skybox_lod; }
	[[nodiscard]] Vec3 get_tint() const noexcept { return m_tint; }
	[[nodiscard]] bool is_dirty() const noexcept { return m_is_dirty; }
	[[nodiscard]] bool should_render_skybox() const noexcept { return m_render_skybox && m_is_active; }
	[[nodiscard]] bool get_render_skybox() const noexcept { return m_render_skybox; }

	/// Unit vector toward the sun, y up.
	[[nodiscard]] Vec3 get_sun_direction() const noexcept {
		const F32 elevation = Math::radians(m_sun_elevation);
		const F32 azimuth = Math::radians(m_sun_azimuth);
		const F32 cos_elevation = Math::cos(elevation);
		return Vec3(cos_elevation * Math::sin(azimuth), Math::sin(elevation), cos_elevation * Math::cos(azimuth));
	}

	[[nodiscard]] const Ref<GFX::GfxTexture> &get_hdr_texture() const noexcept { return m_hdr_texture; }
	[[nodiscard]] const SHCoefficients &get_irradiance() const noexcept { return m_irradiance; }

	void set_active(bool active) noexcept { m_is_active = active; }
	void set_source(SkySource source) noexcept {
		m_source = source;
		m_is_dirty = true;
	}
	void set_sun_elevation(F32 elevation) noexcept {
		m_sun_elevation = elevation;
		m_is_dirty = true;
	}
	void set_sun_azimuth(F32 azimuth) noexcept {
		m_sun_azimuth = azimuth;
		m_is_dirty = true;
	}
	void set_turbidity(F32 turbidity) noexcept {
		m_turbidity = turbidity;
		m_is_dirty = true;
	}
	void set_ground_albedo(const Vec3 &albedo) noexcept {
		m_ground_albedo = albedo;
		m_is_dirty = true;
	}
	void set_intensity(F32 intensity) noexcept { m_intensity = intensity; }
	void set_skybox_lod(F32 lod) noexcept { m_skybox_lod = lod; }
	void set_tint(const Vec3 &tint) noexcept { m_tint = tint; }
	void set_render_skybox(bool render) noexcept { m_render_skybox = render; }
	void mark_clean() noexcept { m_is_dirty = false; }

	void set_hdr_texture(const Ref<GFX::GfxTexture> &texture) {
		m_hdr_texture = texture;
		m_is_dirty = true;
	}

	void set_irradiance(const SHCoefficients &sh) {
		m_irradiance = sh;
		m_is_dirty = false;
	}

	SkyLightComponent() = default;
	explicit SkyLightComponent(const Ref<GFX::GfxTexture> &texture, F32 intensity = 1.0F, int lod = 0.0F)
		: m_hdr_texture(texture), m_source(SkySource::HdrImage), m_intensity(intensity), m_skybox_lod(lod) {}
};

} // namespace Aquila::SceneManagement::Components

#endif // SKYLIGHT_COMPONENT_H
