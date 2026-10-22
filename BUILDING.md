# Building Aquila

`cmake --preset <name>`, then `cmake --build --preset <name>`. `cmake --build build/<name>` builds everything, tests included.

## Presets

- `editor-debug`: day to day. Validation layers, shader hot reload, standard library bounds checks.
- `editor-relwithdebinfo`: optimized with the same debugging help, for profiling.
- `editor-release`: runs like the shipped editor (ThinLTO, precompiled shaders, shared engine).
- `engine-*` is the same without the editor, `*-gcc` the same with gcc.

On Windows the engine is always linked statically.

## Shipping

```bash
cmake --workflow --preset editor-shipping
```

Packs `build/editor-shipping/AquilaStudio-<version>-<os>-<arch>.tar.gz`, unpacked in `build/editor-shipping/AquilaStudio`. It doesn't need the source tree and writes projects and preferences inside its own folder.

## Shaders

Release and shipping bake the engine shaders at build time and ship no shader source. A shader that doesn't compile fails the build, `AQUILA_SHADER_BAKE_SKIP` lists the dead ones it leaves out. Games built from the editor always get their shaders baked.

## Checks (Tested on Linux only for now)

- `editor-asan`: ASan and UBSan, runs the tests and the editor runs with them too. `UBSAN_OPTIONS=halt_on_error=0` keeps going after a report.
- `engine-tsan`: CPU tests only, TSan doesn't work with the NVIDIA driver.
- `editor-coverage`: report in `build/editor-coverage/Coverage/html/index.html`.
- `editor-tidy`: clang-tidy on everything, slow.
- `engine-fuzz`: target `Fuzz` runs every fuzzer for 60 s. A minimized crash in `Engine/Tests/Fuzz/Corpus/<Name>/` becomes a regression test.

`FormatDiff` and `TidyDiff` only check what changed since `develop`.

## Tests

`ctest --preset editor-debug`. Run `PlatformTests` from the build folder, it leaves `__test_*` folders behind.

## Git hooks

Before a commit they format the staged C++ lines, lint the UI markup and refuse whitespace errors and non-https submodule URLs. Before a push they refuse a submodule pinned to a commit its remote doesn't have. `AQUILA_GIT_HOOKS=OFF` turns them off.
