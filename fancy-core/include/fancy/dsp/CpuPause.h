#pragma once

// A spin-wait hint for loops that wait a few microseconds for another thread: `pause` on x86,
// `yield` on ARM (Apple silicon). docs/mac.md, step 1.

#if defined (__x86_64__) || defined (_M_X64) || defined (__i386__) || defined (_M_IX86)
 #include <immintrin.h>
#else
 #include <thread>
#endif

namespace fancy::dsp
{

inline void cpuPause() noexcept
{
   #if defined (__x86_64__) || defined (_M_X64) || defined (__i386__) || defined (_M_IX86)
    _mm_pause();
   #elif defined (__aarch64__) || defined (__arm64__)
    __asm__ __volatile__ ("yield");
   #else
    std::this_thread::yield();
   #endif
}

} // namespace fancy::dsp
