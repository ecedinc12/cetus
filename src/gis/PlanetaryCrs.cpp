#include "gis/PlanetaryCrs.h"

namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kRadiansPerDegree = kPi / 180.0;
}  // namespace

QString PlanetaryCrs::marsEquirectangular(double centralMeridianDegrees)
{
    // 'g'/12 keeps typical lon_0 values (e.g. 137.44) in plain decimal form;
    // 'f'/0 renders the radius as an integer (3396190, never 3.39619e+06).
    return QStringLiteral("+proj=eqc +lat_ts=0 +lon_0=%1 +R=%2 +units=m +no_defs")
        .arg(QString::number(centralMeridianDegrees, 'g', 12),
             QString::number(kMarsRadiusM, 'f', 0));
}

PlanetaryCrs::LonLat PlanetaryCrs::eqcToLonLat(double eastingM,
                                               double northingM,
                                               double centralMeridianDegrees)
{
    LonLat out;
    out.lonDeg = centralMeridianDegrees +
                 (eastingM / kMarsRadiusM) / kRadiansPerDegree;
    out.latDeg = (northingM / kMarsRadiusM) / kRadiansPerDegree;
    return out;
}

QPointF PlanetaryCrs::lonLatToEqc(double lonDeg, double latDeg,
                                  double centralMeridianDegrees)
{
    const double x = kMarsRadiusM * (lonDeg - centralMeridianDegrees) *
                     kRadiansPerDegree;
    const double y = kMarsRadiusM * latDeg * kRadiansPerDegree;
    return QPointF(x, y);
}
