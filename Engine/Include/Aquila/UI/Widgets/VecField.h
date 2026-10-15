#pragma once

#include "Aquila/UI/Widgets/Control.h"
#include "Aquila/UI/Widgets/DragFloat.h"
#include "Aquila/UI/Widgets/Label.h"

namespace Aquila::UI::Core {

class LabeledDragFloat : public View {
  public:
	explicit LabeledDragFloat(const char *label);

	DragFloat *get_drag() { return m_drag; }
	Label *get_label() { return m_label; }

  private:
	Label *m_label = nullptr;
	DragFloat *m_drag = nullptr;
};

template <int N, typename VecT> class VecFieldBase : public Control {
  public:
	void set_value(VecT v) {
		for (int i = 0; i < N; ++i) {
			m_components[i]->set_value(v[i]);
		}
	}

	[[nodiscard]] VecT get_value() const {
		VecT v{};
		for (int i = 0; i < N; ++i) {
			v[i] = m_components[i]->get_value();
		}
		return v;
	}

	void set_step(float step) {
		for (int i = 0; i < N; ++i) {
			m_components[i]->set_step(step);
		}
	}

	void set_speed(float speed) {
		for (int i = 0; i < N; ++i) {
			m_components[i]->set_speed(speed);
		}
	}

	void set_stacked(bool stacked) {
		constexpr const char *k_axes[] = { "X", "Y", "Z", "W" };
		set_class("vec-field-stacked", stacked);
		for (int i = 0; i < N; ++i) {
			DragFloat *drag = m_components[i];
			drag->set_label(stacked ? k_axes[i] : "");
			if (stacked) {
				drag->set_prefix("");
			}
			drag->set_class("vec-stack-first", stacked && i == 0);
			drag->set_class("vec-stack-middle", stacked && i > 0 && i < N - 1);
			drag->set_class("vec-stack-last", stacked && i == N - 1);
			if (auto *labeled = dynamic_cast<LabeledDragFloat *>(drag->get_parent())) {
				labeled->get_label()->set_hidden(stacked);
				labeled->set_class("labeled-drag-stacked", stacked);
			}
		}
	}

	void set_suffix(const std::string &suffix) {
		for (int i = 0; i < N; ++i) {
			m_components[i]->set_suffix(suffix);
		}
	}

	void set_precision(int decimals) {
		for (int i = 0; i < N; ++i) {
			m_components[i]->set_precision(decimals);
		}
	}

	Signal<void(VecT)> on_changed;

  protected:
	void register_component(int index, DragFloat *drag) {
		m_components[index] = drag;
		drag->on_changed.connect([this](float) { on_changed(get_value()); });
	}

  private:
	DragFloat *m_components[N] = {};
};

class Vec2Field : public VecFieldBase<2, Vec2> {
  public:
	Vec2Field();
	[[nodiscard]] std::string_view get_type_name() const override { return "Vec2Field"; }
};

class Vec3Field : public VecFieldBase<3, Vec3> {
  public:
	Vec3Field();
	[[nodiscard]] std::string_view get_type_name() const override { return "Vec3Field"; }
};

class Vec4Field : public VecFieldBase<4, Vec4> {
  public:
	Vec4Field();
	[[nodiscard]] std::string_view get_type_name() const override { return "Vec4Field"; }
};

} // namespace Aquila::UI::Core
