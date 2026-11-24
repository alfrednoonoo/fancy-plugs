// Prints each test case's name to stderr, flushed, before it runs, so a crash that kills the
// process without a Catch2 report (heap corruption, 0xc0000374, runs 45175938464, 45301827449
// and 45307395962) names the test it happened in. `--durations yes` prints only after a test ends.

#include <catch2/catch_test_case_info.hpp>
#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>

#include <cstdio>

namespace
{
class TestStartListener final : public Catch::EventListenerBase
{
public:
    using Catch::EventListenerBase::EventListenerBase;

    void testCaseStarting (const Catch::TestCaseInfo& info) override
    {
        std::fprintf (stderr, "START: %s\n", info.name.c_str());
        std::fflush (stderr);
    }
};
} // namespace

CATCH_REGISTER_LISTENER (TestStartListener)
