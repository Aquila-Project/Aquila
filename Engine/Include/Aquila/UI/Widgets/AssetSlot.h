#pragma once

#include "Aquila/UI/Core/View.h"
#include "Aquila/UI/Widgets/AssetPayload.h"
#include "Aquila/UI/Widgets/Label.h"
#include "Aquila/UI/Widgets/Button.h"
#include "Aquila/UI/Core/DragState.h"

namespace Aquila::UI::Core {

/// A drop target for one asset. An empty accepted type takes anything.
/// on_changed gets nullopt when the slot is cleared.
class AssetSlot : public View {
  public:
	/// Lets slots of one type take drags that aren't AssetPayloads and name the values they store. The editor
	/// registers "entity": hierarchy rows drag an Entity, the slot stores its UUID and shows its name.
	struct Adapter {
		Delegate<Option<AssetPayload>(const std::any &payload)> from_drag;
		/// The name to show for a stored asset_path.
		Delegate<std::string(const std::string &asset_path)> display_name;
	};
	/// An empty adapter removes it. Main thread.
	static void set_adapter(const std::string &type, Adapter adapter);
	[[nodiscard]] static const Adapter *find_adapter(std::string_view type);
	AssetSlot();
	explicit AssetSlot(std::string accepted_type);

	[[nodiscard]] std::string_view get_type_name() const override { return "AssetSlot"; }

	void set_accepted_type(std::string type);
	[[nodiscard]] const std::string &get_accepted_type() const { return m_accepted_type; }

	void set_placeholder(std::string text);

	void set_value(AssetPayload asset);
	void clear();
	[[nodiscard]] bool has_value() const { return m_has_value; }
	[[nodiscard]] const AssetPayload &get_value() const { return m_value; }

	Signal<void(Option<AssetPayload>)> on_changed;

	void on_drop(DragState &state) override;
	void on_drag_enter(DragState &state) override;
	void on_drag_leave(DragState &state) override;
	void apply_xml_attribute(std::string_view name, std::string_view value, IResourceResolver *resolver = nullptr) override;

  private:
	void update_display();
	void show_hint(std::string text);
	[[nodiscard]] std::string accepted_name() const;
	[[nodiscard]] bool is_compatible(const AssetPayload &payload) const;
	[[nodiscard]] Option<AssetPayload> read_payload(const std::any &payload) const;

	std::string m_accepted_type;
	std::string m_placeholder;
	AssetPayload m_value;
	bool m_has_value = false;

	Label *m_label = nullptr;
	Button *m_clear_button = nullptr;

};

} // namespace Aquila::UI::Core
