#ifndef AQUILA_TESTS_UI_LOADER_TEST_SUPPORT_H
#define AQUILA_TESTS_UI_LOADER_TEST_SUPPORT_H

#include "Aquila/UI/Core/Canvas.h"
#include "Aquila/UI/Core/LayoutLoader.h"
#include "CanvasTestSupport.h"
#include "VfsTestSupport.h"

inline std::vector<std::string> editor_base_styles() {
	return { "/resources/Engine/UI/tokens.aqstyle", "/resources/Engine/UI/widgets.aqstyle",
			 "/resources/Engine/UI/dock.aqstyle" };
}

struct LoaderCanvas {
	CanvasSingletonsScope singletons;
	Aquila::UI::Core::LayoutLoader loader;
	Unique<Aquila::UI::Core::Canvas> canvas;

	explicit LoaderCanvas(Uint32 width = 800, Uint32 height = 600,
						  std::vector<std::string> base_styles = editor_base_styles()) {
		test_vfs();
		loader.styles().set_base(std::move(base_styles));
		canvas = std::make_unique<Aquila::UI::Core::Canvas>(width, height);
		canvas->set_layout_loader(&loader);
	}

	[[nodiscard]] Aquila::UI::Core::View &host() const { return *canvas->get_root(); }

	void settle() const {
		for (int i = 0; i < 4; ++i) {
			canvas->update(0.016F);
			canvas->compute();
		}
	}

	void settle_fully() const {
		settle();
		std::this_thread::sleep_for(std::chrono::milliseconds(400));
		settle();
	}
};

#endif
