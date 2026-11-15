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
        // What Core Audio gives its I/O threads: a period of about one 128-sample block at
        // 44.1 kHz, of which up to half is computation.
        mach_timebase_info_data_t timebase {};
        mach_timebase_info (&timebase);
        const auto ticks = [&] (double seconds)
        {
            return (uint32_t) (seconds * 1.0e9 * (double) timebase.denom / (double) timebase.numer);
        };

        thread_time_constraint_policy_data_t policy {};
        policy.period = ticks (0.0029);
        policy.computation = ticks (0.0015);
        policy.constraint = ticks (0.0029);
        policy.preemptible = 1;
        thread_policy_set (thread, THREAD_TIME_CONSTRAINT_POLICY, (thread_policy_t) &policy, THREAD_TIME_CONSTRAINT_POLICY_COUNT);
    }
    else
    {
        thread_standard_policy_data_t policy {};
        thread_policy_set (thread, THREAD_STANDARD_POLICY, (thread_policy_t) &policy, THREAD_STANDARD_POLICY_COUNT);
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

    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds (200))
        fancy::dsp::cpuPause();

    const auto endCycles = threadCycles();
    const std::chrono::duration<double> wall = std::chrono::steady_clock::now() - start;

    return (double) (endCycles - startCycles) / wall.count();
}

} // namespace bench::platform
