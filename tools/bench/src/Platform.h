#pragma once

// What FancyBench and the CPU tests need from the operating system (docs/mac.md, step 1): thread
// priority, CPU time, a thread cycle counter, short waits and the command line. Win32 on Windows;
// Mach and POSIX on macOS.

#include <juce_core/juce_core.h>

#include <cstdint>

namespace bench::platform
{

// Raises the calling thread to the priority a host's audio thread has (Windows: time critical;
// macOS: a Mach time-constraint thread), or puts it back to normal.
void setAudioPriority (bool on);

struct ScopedAudioPriority
{
    explicit ScopedAudioPriority (bool on = true) : raised (on) { if (raised) setAudioPriority (true); }
    ~ScopedAudioPriority() { if (raised) setAudioPriority (false); }
    ScopedAudioPriority (const ScopedAudioPriority&) = delete;
    ScopedAudioPriority& operator= (const ScopedAudioPriority&) = delete;

    const bool raised;
};

// CPU time used so far, user plus system: by the calling thread, and by the whole process.
double threadCpuSeconds();
double processCpuSeconds();

// A fine counter of the calling thread's own running time, in units only cyclesPerSecond()
// converts (Windows: cycles; macOS: nanoseconds).
std::uint64_t threadCycles();

// Measured by spinning for 200 ms.
double cyclesPerSecond();

// About a millisecond's sleep, and giving up the rest of the time slice.
void sleepBriefly();
void yieldThread();

// One-millisecond timer resolution while it exists (Windows); nothing elsewhere.
struct ScopedTimerResolution
{
    ScopedTimerResolution();
    ~ScopedTimerResolution();
    ScopedTimerResolution (const ScopedTimerResolution&) = delete;
    ScopedTimerResolution& operator= (const ScopedTimerResolution&) = delete;
};

// UTF-8 console output, and no crash dialogs in a child process (Windows); nothing elsewhere.
void prepareConsole();
void suppressCrashDialogs();

// The command-line arguments after the program name, with paths outside the ANSI code page intact
// on Windows.
juce::StringArray arguments (int argc, char* argv[]);

} // namespace bench::platform
