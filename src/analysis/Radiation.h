#pragma once

// Third measured data kind: surface radiation (MSL RAD, Curiosity, Gale Crater).
//
// NASA-verified number: Hassler et al., Science 343, 1244797 (2014),
// doi:10.1126/science.1244797. Dose equivalent rate on the Martian surface
// 0.64 +/- 0.12 mSv/day (GCR dominated, Aug 2012 - Jun 2013, measured inside
// the rover, i.e. under rover shielding).
//
// What is derived here (not a NASA value): the dose accumulated over an EVA is
// rate * EVA duration. No suit shielding model, no solar-particle-event model
// and no dose limit are claimed; this is an exposure estimate for planning.

namespace Radiation
{
// Measured dose equivalent rate, mSv per day (MSL RAD, Hassler 2014).
constexpr double kDoseRateMsvPerDay = 0.64;
// Quoted measurement uncertainty, mSv per day (same source).
constexpr double kDoseRateSigmaMsvPerDay = 0.12;
// Minutes in one Earth day (the paper's "day" is a 24 h interval).
constexpr double kMinutesPerDay = 1440.0;

// Estimated dose equivalent for an EVA of the given length, mSv. A negative
// duration is invalid input and yields 0 (never a negative dose).
inline double evaDoseMsv(double evaMinutes, double rateMsvPerDay = kDoseRateMsvPerDay)
{
    return evaMinutes > 0.0 ? rateMsvPerDay * evaMinutes / kMinutesPerDay : 0.0;
}
}  // namespace Radiation
