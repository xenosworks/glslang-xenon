// Copyright (C) 2026 The glslang-xenon contributors
// SPDX-License-Identifier: BSD-3-Clause
//
// Compiles every GLSL program of Dusklight's shader pack on the Xbox 360 through glslang, as `glslang -V` does on the
// PC (README-xsdk.md): Vulkan 1.0 rules, SPIR-V 1.0, no optimiser. Logs each program's time and whether its SPIR-V is
// the one the PC's glslang made, then the heap and stack compiling took, PASS or FAIL for each check, and
// "tests: <passed> passed, <failed> failed".
//
// Built with GLSLANG_XSDK_BREAK, it targets SPIR-V 1.3 instead, so no program's SPIR-V is the PC's: the check that
// compares them is broken on purpose.

#include "SPIRV/GlslangToSpv.h"
#include "glslang/Public/ResourceLimits.h"
#include "glslang/Public/ShaderLang.h"

#include <malloc.h>
#include <pthread.h>
#include <time.h>

#include <cstdint>
#include <cstdio>
#include <vector>

#include "programs.h"

namespace {

// The thread that compiles: its stack, the part of it next to its first frame left unpainted, as the kernel may keep
// some of the stack's top for itself, and a margin below the frame that paints.
constexpr size_t StackBytes = 1024 * 1024;
constexpr size_t StackTopMargin = 64 * 1024;
constexpr size_t PaintMargin = 4 * 1024;
constexpr uint32_t Paint = 0x5A17C0DEu;

#ifdef GLSLANG_XSDK_BREAK
constexpr glslang::EShTargetLanguageVersion TargetSpirv = glslang::EShTargetSpv_1_3;
#else
constexpr glslang::EShTargetLanguageVersion TargetSpirv = glslang::EShTargetSpv_1_0;
#endif

constexpr size_t ProgramCount = sizeof(programs) / sizeof(programs[0]);

// The first program compiled also has glslang set up its built-in symbols, so it's timed apart from the rest.
struct Results {
    unsigned compiled = 0;
    unsigned same = 0;
    uint64_t firstUs = 0;
    const char* first = "";
    uint64_t restUs = 0;
    uint64_t slowestUs = 0;
    const char* slowest = "";
    size_t heapBefore = 0;
    size_t heapMost = 0;
    size_t arenaMost = 0;
    size_t stackMost = 0;
    size_t painted = 0;
};

uint64_t nowUs()
{
    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return uint64_t(now.tv_sec) * 1000000 + uint64_t(now.tv_nsec) / 1000;
}

// FNV-1a over each word's bytes, low byte first, as vulkan4xenon hashes SPIR-V.
uint64_t hashSpirv(const std::vector<unsigned int>& spirv)
{
    uint64_t value = 0xCBF29CE484222325ull;
    for (unsigned int word : spirv) {
        for (int shift = 0; shift < 32; shift += 8)
            value = (value ^ ((word >> shift) & 0xFF)) * 0x100000001B3ull;
    }
    return value;
}

// One program, as glslang's standalone compiler takes it with -V: parsed, linked alone, then turned into SPIR-V.
bool compile(const Program& program, std::vector<unsigned int>& spirv)
{
    const EShMessages messages = EShMessages(EShMsgSpvRules | EShMsgVulkanRules);
    glslang::TShader shader(program.stage);
    shader.setStrings(&program.source, 1);
    shader.setEnvInput(glslang::EShSourceGlsl, program.stage, glslang::EShClientVulkan, 100);
    shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_0);
    shader.setEnvTarget(glslang::EShTargetSpv, TargetSpirv);
    if (!shader.parse(GetDefaultResources(), 100, false, messages)) {
        printf("glslang: %s: %s\n", program.name, shader.getInfoLog());
        return false;
    }
    glslang::TProgram linked;
    linked.addShader(&shader);
    if (!linked.link(messages)) {
        printf("glslang: %s: %s\n", program.name, linked.getInfoLog());
        return false;
    }
    glslang::SpvOptions options;
    options.disableOptimizer = false;
    glslang::GlslangToSpv(*linked.getIntermediate(program.stage), spirv, &options);
    return true;
}

// Paints the stack below this function's frame down to bottom, so what compiling writes there shows.
__attribute__((noinline)) void paintStack(uintptr_t bottom)
{
    const uintptr_t here = uintptr_t(__builtin_frame_address(0)) - PaintMargin;
    for (volatile uint32_t* word = reinterpret_cast<volatile uint32_t*>(bottom); uintptr_t(word) < here; ++word)
        *word = Paint;
}

void* compileAll(void* argument)
{
    Results& results = *static_cast<Results*>(argument);
    const uintptr_t top = uintptr_t(__builtin_frame_address(0));
    const uintptr_t bottom = top - StackBytes + StackTopMargin;
    paintStack(bottom);
    results.painted = top - PaintMargin - bottom;

    for (const Program& program : programs) {
        std::vector<unsigned int> spirv;
        const uint64_t start = nowUs();
        const bool compiled = compile(program, spirv);
        const uint64_t us = nowUs() - start;
        const struct mallinfo heap = mallinfo();
        if (heap.uordblks > results.heapMost)
            results.heapMost = heap.uordblks;
        if (heap.arena > results.arenaMost)
            results.arenaMost = heap.arena;
        const bool same = compiled && spirv.size() == program.pcWords && hashSpirv(spirv) == program.pcHash;
        results.compiled += compiled;
        results.same += same;
        if (&program == &programs[0]) {
            results.firstUs = us;
            results.first = program.name;
        } else
            results.restUs += us;
        if (&program != &programs[0] && us > results.slowestUs) {
            results.slowestUs = us;
            results.slowest = program.name;
        }
        printf("glslang: %s: %llu us, %u words, %s\n", program.name, (unsigned long long)us, unsigned(spirv.size()),
               !compiled ? "FAILED"
               : same    ? "the PC's SPIR-V"
                         : "NOT the PC's SPIR-V");
    }

    const volatile uint32_t* word = reinterpret_cast<const volatile uint32_t*>(bottom);
    while (uintptr_t(word) < top - PaintMargin && *word == Paint)
        ++word;
    results.stackMost = top - uintptr_t(word);
    return nullptr;
}

} // anonymous namespace

int main()
{
    printf("glslang probe: %u programs, target SPIR-V %s%s\n", unsigned(ProgramCount),
           TargetSpirv == glslang::EShTargetSpv_1_0 ? "1.0" : "1.3",
           TargetSpirv == glslang::EShTargetSpv_1_0 ? "" : " (broken on purpose)");
    Results results;
    results.heapBefore = mallinfo().uordblks;
    const uint64_t initStart = nowUs();
    glslang::InitializeProcess();
    const uint64_t initUs = nowUs() - initStart;

    pthread_attr_t attributes;
    pthread_attr_init(&attributes);
    pthread_attr_setstacksize(&attributes, StackBytes);
    pthread_t thread;
    if (pthread_create(&thread, &attributes, compileAll, &results) != 0) {
        printf("FAIL: glslang: the compiling thread wasn't made\n");
        printf("tests: 0 passed, 1 failed\n");
        return 1;
    }
    pthread_join(thread, nullptr);
    pthread_attr_destroy(&attributes);
    glslang::FinalizeProcess();

    printf("glslang: start-up %llu us; the first program, %s, with glslang's built-in symbols, %llu us; the other %u "
           "in %llu us, %llu us each on average, the slowest %s at %llu us\n",
           (unsigned long long)initUs, results.first, (unsigned long long)results.firstUs, unsigned(ProgramCount - 1),
           (unsigned long long)results.restUs, (unsigned long long)(results.restUs / (ProgramCount - 1)),
           results.slowest, (unsigned long long)results.slowestUs);
    printf("glslang: heap in use %u KB before, %u KB at most after a program; the heap grew to %u KB at most\n",
           unsigned(results.heapBefore / 1024), unsigned(results.heapMost / 1024), unsigned(results.arenaMost / 1024));
    printf("glslang: stack %u KB at most below the compiling thread's first frame, of %u KB watched\n",
           unsigned(results.stackMost / 1024), unsigned(results.painted / 1024));

    int passed = 0;
    int failed = 0;
    const bool allCompiled = results.compiled == ProgramCount;
    const bool allSame = results.same == ProgramCount;
    printf("%s: glslang: %u of %u programs compiled\n", allCompiled ? "PASS" : "FAIL", results.compiled,
           unsigned(ProgramCount));
    printf("%s: glslang: %u of %u programs' SPIR-V is the PC's\n", allSame ? "PASS" : "FAIL", results.same,
           unsigned(ProgramCount));
    passed += allCompiled + allSame;
    failed += !allCompiled + !allSame;
    printf("tests: %d passed, %d failed\n", passed, failed);
    return 0;
}
