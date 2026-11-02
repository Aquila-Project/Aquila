#pragma once

#include "Aquila/UI/Widgets/BaseField.h"

namespace Aquila::UI::Core {

class Toggle : public BaseField<bool> {
  public:
	Toggle();
	explicit Toggle(bool on);

	[[nodiscard]] std::string_view get_type_name() const override { return "Toggle"; }

	void set_on(bool on) { set_value(on); }
	[[nodiscard]] bool is_on() const { return get_value(); }

	void apply_xml_attribute(std::string_view name, std::string_view value,
							 IResourceResolver *resolver = nullptr) override;
	void on_mouse_release(Platform::MouseButton btn, Vec2 pos) override;
	void on_draw_self(Rendering::DrawList &draw_list) override;

  protected:
	void on_value_updated() override;
};

} // namespace Aquila::UI::Core
