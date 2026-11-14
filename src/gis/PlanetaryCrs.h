#pragma once

#include <QPointF>
#include <QString>

// Mars body constants and CRS helpers (AGENTS.md §5: Mars geodesy, never
// Earth WGS84 / Web Mercator).
class PlanetaryCrs
{
public:
    // Mars mean radius in metres. IAU Mars 2015 body (PLAN.md §4).
    static constexpr double kMarsRadiusM = 3396190.0;

    // EPSG-less PROJ string for a Mars equirectangular projection whose
    // central meridian is `centralMeridianDegrees`, e.g. Gale Crater sits at
    // 137.44°E:
    //   +proj=eqc +lat_ts=0 +lon_0=137.44 +R=3396190 +units=m +no_defs
    static QString marsEquirectangular(double centralMeridianDegrees);

    // Geographic position in degrees.
    struct LonLat
    {
        double lonDeg;
        double latDeg;
    };

    // Inverse of lonLatToEqc(): projected metres -> lon/lat degrees.
    // Sphere with lat_ts = 0 (see lonLatToEqc).
    static LonLat eqcToLonLat(double eastingM, double northingM,
                              double centralMeridianDegrees);

    // Forward equirectangular projection on the Mars sphere (R =
    // kMarsRadiusM, lat_ts = 0):
    //   x = R * (lon - lon0) * pi/180
    //   y = R * lat * pi/180
    static QPointF lonLatToEqc(double lonDeg, double latDeg,
                               double centralMeridianDegrees);
};
