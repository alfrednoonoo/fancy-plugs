#if defined (_WIN32)
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
 #include <mmsystem.h>
 #include <shellapi.h>
 #pragma comment(lib, "winmm.lib")
 #pragma comment(lib, "shell32.lib")
#else
 #include <pthread.h>
 #include <sched.h>
 #include <sys/resource.h>
 #include <time.h>
 #include <unistd.h>
 #if defined (__APPLE__)
  #include <mach/mach.h>
  #include <mach/mach_time.h>
  #include <mach/thread_policy.h>
  #include <pthread/qos.h>
 #endif
#endif

#include "Platform.h"

#include <fancy/dsp/CpuPause.h>

#include <chrono>

namespace bench::platform
{

#if defined (_WIN32)

namespace
{
double secondsOf (const FILETIME& time)
{
    return (double) (((std::uint64_t) time.dwHighDateTime << 32) | time.dwLowDateTime) * 1.0e-7;
}
} // namespace

void setAudioPriority (bool on)
{
    SetThreadPriority (GetCurrentThread(), on ? THREAD_PRIORITY_TIME_CRITICAL : THREAD_PRIORITY_NORMAL);
}

double threadCpuSeconds()
{
    FILETIME created, exited, kernel, user;

    if (! GetThreadTimes (GetCurrentThread(), &created, &exited, &kernel, &user))
        return 0.0;

    return secondsOf (kernel) + secondsOf (user);
}

double processCpuSeconds()
{
    FILETIME created, exited, kernel, user;

    if (! GetProcessTimes (GetCurrentProcess(), &created, &exited, &kernel, &user))
        return 0.0;

    return secondsOf (kernel) + secondsOf (user);
}

std::uint64_t threadCycles()
{
    ULONG64 cycles = 0;
    QueryThreadCycleTime (GetCurrentThread(), &cycles);
    return (std::uint64_t) cycles;
}

void sleepBriefly() { Sleep (1); }
void yieldThread() { SwitchToThread(); }

ScopedTimerResolution::ScopedTimerResolution() { timeBeginPeriod (1); }
ScopedTimerResolution::~ScopedTimerResolution() { timeEndPeriod (1); }

void prepareConsole() { SetConsoleOutputCP (CP_UTF8); }

void setEnvironment (const juce::String& name, const juce::String& value)
{
    SetEnvironmentVariableW (name.toWideCharPointer(), value.toWideCharPointer());
}
void suppressCrashDialogs() { SetErrorMode (SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX); }

juce::StringArray arguments (int, char*[])
{
    // The wide command line, so paths outside the ANSI code page survive.
    juce::StringArray args;
    int count = 0;

    if (auto** wide = CommandLineToArgvW (GetCommandLineW(), &count))
    {
        for (int i = 1; i < count; ++i)
            args.add (juce::String (wide[i]));

        LocalFree (wide);
    }

    return args;
}

#else

namespace
{
double secondsOf (const timeval& time) { return (double) time.tv_sec + 1.0e-6 * (double) time.tv_usec; }
} // namespace

void setAudioPriority (bool on)
{
   #if defined (__APPLE__)
    const auto thread = pthread_mach_thread_np (pthread_self());

    if (on)
    {
        // The highest quality-of-service class. Not a Mach time-constraint thread, which is what
        // Core Audio's I/O threads are: the benchmarks spin while they wait for the next block,
        // and the kernel demotes a time-constraint thread that runs past its computation budget,
        // which preempted 127 of 1000 blocks in the first Mac run (43765631996).
        juce::ignoreUnused (thread);
        pthread_set_qos_class_self_np (QOS_CLASS_USER_INTERACTIVE, 0);
    }
    else
    {
        juce::ignoreUnused (thread);
        pthread_set_qos_class_self_np (QOS_CLASS_DEFAULT, 0);
    }
   #else
    juce::ignoreUnused (on);
   #endif
}

double threadCpuSeconds()
{
    timespec time {};
    clock_gettime (CLOCK_THREAD_CPUTIME_ID, &time);
    return (double) time.tv_sec + 1.0e-9 * (double) time.tv_nsec;
}

double processCpuSeconds()
{
    rusage usage {};
    getrusage (RUSAGE_SELF, &usage);
    return secondsOf (usage.ru_utime) + secondsOf (usage.ru_stime);
}

std::uint64_t threadCycles()
{
    timespec time {};
    clock_gettime (CLOCK_THREAD_CPUTIME_ID, &time);
    return (std::uint64_t) time.tv_sec * 1000000000u + (std::uint64_t) time.tv_nsec;
}

void sleepBriefly() { usleep (1000); }
void yieldThread() { sched_yield(); }

ScopedTimerResolution::ScopedTimerResolution() {}
ScopedTimerResolution::~ScopedTimerResolution() {}

void prepareConsole() {}

void setEnvironment (const juce::String& name, const juce::String& value)
{
    setenv (name.toRawUTF8(), value.toRawUTF8(), 1);
}
void suppressCrashDialogs() {}

juce::StringArray arguments (int argc, char* argv[])
{
    juce::StringArray args;

    for (int i = 1; i < argc; ++i)
        args.add (juce::String::fromUTF8 (argv[i]));

    return args;
}

#endif

double cyclesPerSecond()
{
    const auto start = std::chrono::steady_clock::now();
    const auto startCycles = threadCycles();

    // Busy, without a pause instruction: under Rosetta `pause` gives the core away, and the
    // thread then used 5 ms of CPU in 200 ms (run 43765631996).
    volatile std::uint32_t spin = 0;

    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds (200))
        for (int i = 0; i < 64; ++i)
            spin = spin + 1;

    const auto endCycles = threadCycles();
    const std::chrono::duration<double> wall = std::chrono::steady_clock::now() - start;

    return (double) (endCycles - startCycles) / wall.count();
}

} // namespace bench::platform
