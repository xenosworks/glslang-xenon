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

### On the PC, for HLSL

A title's HLSL goes through glslang on the PC, with SPIR-V's optimiser, which legalises what the HLSL front end
writes (`SPIRV/GlslangToSpv.cpp`, `SpirvToolsTransform`). Build SPIRV-Tools and SPIRV-Headers at the revisions
`known_good.json` names, then glslang against them:

```bash
git clone https://github.com/KhronosGroup/SPIRV-Tools.git && git -C SPIRV-Tools checkout 2ec8457ab33d539b6f1fecc998360c0b8b05ed4f
git clone https://github.com/KhronosGroup/SPIRV-Headers.git && git -C SPIRV-Headers checkout 1a22b167081842915a1c78a0b5b5a353a23284aa
cmake -S SPIRV-Tools -B spirv-tools-build -GNinja -DCMAKE_BUILD_TYPE=Release -DSPIRV-Headers_SOURCE_DIR=$PWD/SPIRV-Headers \
    -DSPIRV_SKIP_TESTS=ON -DSPIRV_SKIP_EXECUTABLES=ON -DCMAKE_INSTALL_PREFIX=$PWD/install
cmake --build spirv-tools-build --target install
cmake -S <glslang-xenon> -B build -GNinja -DCMAKE_BUILD_TYPE=Release -DENABLE_OPT=ON -DALLOW_EXTERNAL_SPIRV_TOOLS=ON \
    -DCMAKE_PREFIX_PATH=$PWD/install -DGLSLANG_TESTS=OFF
cmake --build build
```

`build/StandAlone/glslang` makes the same SPIR-V of GLSL, with `-V`, as Khronos's 16.3.0 built without the optimiser,
which glslang runs only for HLSL or `-Os`: byte for byte for 596 programs of xsdk's, vulkan4xenon's and Dusklight's
shaders, the 27 others refused alike (xsdk:results/2026-10-20-milestone-50/README.md, "Step 7").

## Notes

-   CMake takes xsdk for a Unix (`CMakeLists.txt`, `glslang/CMakeLists.txt`): glslang's Unix OS layer and its
    threads, which xsdk's pthreads give.
-   GLSL to SPIR-V only. `xsdk/` builds without SPIRV-Tools and the HLSL front end: glslang runs its optimiser only for
    HLSL or when asked to optimise for size, so SPIR-V from GLSL comes out as `glslang -V` makes it on the PC.
-   Build with an xsdk toolchain that keeps LR to 32-bit values (xsdk:tools/toolchain/gcc-xsdk.patch).
-   HLSL's `fmod` truncates, as HLSL's does, where glslang made it GLSL's floored `mod`
    (`glslang/HLSL/hlslParseHelper.cpp`, `decomposeIntrinsic`; `glslang/MachineIndependent/Constant.cpp`'s fold).
-   Under `--hlsl-dx9-compatible -fhlsl_functionality1`, a `$Global` member's DX9 boolean or integer register,
    `register(bN)` or `register(iN)`, is a UserSemantic of the member, `"register(b3)"`, beside any semantic
    (`hlslParseHelper.cpp`, `handleRegister`; `TQualifier::hlslRegister`; `SPIRV/GlslangToSpv.cpp`,
    `decorateStructType`), which xsdk's shader compiler puts where D3D9 does; glslang dropped both.
-   `xsdk/glslang_probe` compiles every GLSL program of a shader pack built on the PC, Dusklight's, with `-V`'s
    settings, and checks each program's SPIR-V against the pack's. On the console (2026-10-14) all 145 of Dusklight's
    came out the same as on the PC: the first in 0.94 s, which sets up glslang's built-in symbols, the rest in 11.3 ms
    each on average and 30 ms at most, with 13.5 MB of heap in use at most and 68 KB of stack
    (xsdk:docs/HARDWARE.md, "glslang on the console").
