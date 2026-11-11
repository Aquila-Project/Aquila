# Aquila

This project is my personal Vulkan-based engine built entirely from scratch with the primary goal of learning and exploring graphics programming and scalable (and maintanable) project architecture. It is still a work in progress, and its lacking comments and documentation (I am pretty inconsistent with this :/). It should come pretty soon though.

The initial idea was to keep it on Vulkan, but I am really veering towards integrating multiple graphics API support.

> **This project is for educational purposes only.**
> The current state of the engine is not designed to compete with existing game engines, commercial or open-source, nor is that its goal. The primary intent is personal growth and experimentation in the field of graphics programming.

## Building

You need git, CMake, Ninja and a C++20 compiler. I use clang, gcc should work too. On Linux you also need the X11 dev headers, CMake tells you which ones are missing and how to install them.

```bash
git clone https://github.com/adinte18/Aquila.git
cd Aquila
cmake --preset editor-debug
cmake --build --preset editor-debug
```

The first configure takes a while because it pulls the submodules and downloads Slang. The Vulkan headers and loader are submodules too, so the LunarG Vulkan SDK is only needed for the validation layers. Building also runs the tests.

Presets are `editor-debug`, `editor-release` and `editor-relwithdebinfo`, plus the same three starting with `engine-` if you don't want the editor. In release the engine gets built as a shared library and the executables just link against it.

What each build is for, how to make a shipping build, and the sanitizer, coverage and fuzzing ones are in [BUILDING.md](BUILDING.md).

Some options if you need them: `AQUILA_SYSTEM_VULKAN` uses an installed Vulkan SDK or system loader instead of the submodules, `AQUILA_FETCH_SLANG` always downloads Slang instead of using the SDK's, `AQUILA_VALIDATION_LAYERS` turns the validation layers on or off, and `AQUILA_AUTO_INSTALL` lets CMake install missing system packages for you.

I shifted my work to Linux. Windows should work but I haven't tested the Windows build in a while, let alone this setup. macOS isn't supported just yet, but if there is someone that wants to port it to macOS, you are more than welcome to do so.

## Dependencies

I am trying to keep the engine dependency free (for the most of it), so this list might shrink overtime, but for now we are using quite a few third party libs to help me with mesh loading, windowing, inputs and GPU memory allocation and more.

GLFW, GLM, Assimp, stb, Vulkan-Headers, Vulkan-Loader, VulkanMemoryAllocator and lunasvg are submodules. EnTT, nlohmann json, Clay and doctest are single headers in `Engine/Vendor`. Shaders go through Slang.

Thanks for checking it out!
