#pragma once

#include "Aquila/UI/Widgets/BaseField.h"

namespace Aquila::UI::Core {

class Checkbox : public BaseField<bool> {
  public:
	Checkbox();
	explicit Checkbox(bool checked);

	[[nodiscard]] std::string_view get_type_name() const override { return "Checkbox"; }

	void set_checked(bool checked) { set_value(checked); }
	[[nodiscard]] bool is_checked() const { return get_value(); }

	void apply_xml_attribute(std::string_view name, std::string_view value,
							 IResourceResolver *resolver = nullptr) override;
	void on_mouse_release(Platform::MouseButton btn, Vec2 pos) override;
	void on_draw_self(Rendering::DrawList &draw_list) override;

  protected:
	void on_value_updated() override;
};

} // namespace Aquila::UI::Core
