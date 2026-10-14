# Xbox 360 (xsdk)

glslang for the Xbox 360 through [xsdk](https://github.com/xenosworks/xsdk), a homebrew SDK that builds titles for the
console's retail kernel. This is an altered version of glslang 16.3.0; the changes are on the `xenon` branch.

## Building

Build xsdk first (`make` in its checkout builds `libxsdk.a`), then add glslang to a title's CMake project with
`add_subdirectory` and link `glslang` and, for the default limits, `glslang-default-resource-limits`, as `xsdk/` does:

```bash
cmake -S xsdk -B build-xsdk -GNinja -DCMAKE_TOOLCHAIN_FILE=<xsdk>/cmake/xsdk.cmake -DCMAKE_BUILD_TYPE=Release \
    -DGLSLANG_XSDK_SHADERS=<dusklight-xenon>/platforms/xenon/shaders \
    -DGLSLANG_XSDK_PACK=<dusklight-xenon>/platforms/xenon/glsl_pack.c
cmake --build build-xsdk
```

The library builds static, and a title turns its ELF into a `.xex` with xsdk's `xsdk_add_xex()`.

## Notes

-   CMake takes xsdk for a Unix (`CMakeLists.txt`, `glslang/CMakeLists.txt`): glslang's Unix OS layer and its
    threads, which xsdk's pthreads give.
-   GLSL to SPIR-V only. `xsdk/` builds without SPIRV-Tools and the HLSL front end: glslang runs its optimiser only for
    HLSL or when asked to optimise for size, so SPIR-V from GLSL comes out as `glslang -V` makes it on the PC.
-   Build with an xsdk toolchain that keeps LR to 32-bit values (xsdk:tools/toolchain/gcc-xsdk.patch).
-   `xsdk/glslang_probe` compiles every GLSL program of a shader pack built on the PC, Dusklight's, with `-V`'s
    settings, and checks each program's SPIR-V against the pack's. On the console (2026-10-14) all 145 of Dusklight's
    came out the same as on the PC: the first in 0.94 s, which sets up glslang's built-in symbols, the rest in 11.3 ms
    each on average and 30 ms at most, with 13.5 MB of heap in use at most and 68 KB of stack
    (xsdk:docs/HARDWARE.md, "glslang on the console").
