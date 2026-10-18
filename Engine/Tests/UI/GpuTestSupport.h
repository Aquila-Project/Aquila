#ifndef AQUILA_TESTS_UI_GPU_TEST_SUPPORT_H
#define AQUILA_TESTS_UI_GPU_TEST_SUPPORT_H

#include <doctest.h>

#include <GLFW/glfw3.h>

#include "Aquila/GFX/GfxContext.h"
#include "Aquila/Platform/Filesystem/NativeFileSystem.h"
#include "Aquila/Platform/Filesystem/VirtualFileSystem.h"
#include "Aquila/UI/Core/FontRegistry.h"
#include "Aquila/UI/Text/FontAtlas.h"

struct TestGpu {
	GLFWwindow *window = nullptr;
	Unique<Aquila::GFX::GfxContext> ctx;
	Unique<Aquila::UI::Text::FontAtlas> font;

	TestGpu() {
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
		window = glfwCreateWindow(320, 240, "UITests", nullptr, nullptr);
		REQUIRE(window != nullptr);
		ctx = Aquila::GFX::GfxContext::create(*window);
		REQUIRE(ctx != nullptr);

		Aquila::Platform::Filesystem::VirtualFileSystem::init();
		Aquila::Platform::Filesystem::VirtualFileSystem::get()->mount(
			"/resources",
			std::make_shared<Aquila::Platform::Filesystem::NativeFileSystem>(std::string(AQUILA_SOURCE_DIR) + "/Resources"));

		font = Aquila::UI::Text::FontAtlas::create_from_file(*ctx, "/resources/Engine/Fonts/Outfit/Outfit-Regular.ttf");
		REQUIRE(font != nullptr);
		for (const char *family : { "regular", "medium", "mono", "bold" }) {
			Aquila::UI::Core::FontRegistry::Register(family, font.get());
		}
	}
};

inline TestGpu &test_gpu() {
	static auto *instance = new TestGpu();
	return *instance;
}

#endif
