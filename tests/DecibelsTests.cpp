#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <fancy/Decibels.h>

TEST_CASE ("dB and gain conversions round-trip")
{
    CHECK (fancy::dbToGain (0.0) == Catch::Approx (1.0));
    CHECK (fancy::dbToGain (-20.0) == Catch::Approx (0.1));
    CHECK (fancy::gainToDb (fancy::dbToGain (-12.0)) == Catch::Approx (-12.0));
}

TEST_CASE ("silence reports the dB floor")
{
    CHECK (fancy::gainToDb (0.0) == Catch::Approx (-150.0));
    CHECK (fancy::gainToDb (0.0, -60.0) == Catch::Approx (-60.0));
}
