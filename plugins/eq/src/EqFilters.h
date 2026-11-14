#pragma once

// The band filters now live in fancy-core (Phase 3 Q2.5), shared with Fancy Comp's detector
// EQ. EQ keeps using them under its own names.
#include <fancy/dsp/MatchedFilters.h>

namespace eq
{
using fancy::dsp::matched::BandFilter;
using fancy::dsp::matched::BandSpec;
using fancy::dsp::matched::BandType;
using fancy::dsp::matched::butterworthQ;
using fancy::dsp::matched::Channel;
using fancy::dsp::matched::design;
using fancy::dsp::matched::DynamicSpec;
using fancy::dsp::matched::hasGain;
using fancy::dsp::matched::hasSlope;
using fancy::dsp::matched::maxBands;
using fancy::dsp::matched::maxSections;
using fancy::dsp::matched::Section;
using fancy::dsp::matched::slopeOrders;
using fancy::dsp::matched::supportsDynamics;
} // namespace eq
