#pragma once

// Surface orientation from a prepared Mars DEM. This is terrain context only:
// it never claims a mineral or in-situ composition measurement (M6).

#include "data/RasterLayer.h"

#include <QPointF>
#include <QString>

struct SurfaceOrientation
{
    double upEast = 0.0, upNorth = 0.0, upUp = 1.0;  // unit normal, ENU
    double slopeDeg = 0.0;                           // 0..90
    double aspectDeg = 0.0;                          // downslope, clockwise from N
    bool aspectDefined = false;                      // false on flat ground
    QString method;                                  // human-readable
};

class TerrainOrientation
{
public:
    // Samples the 3x3 cell neighbourhood around `eastingNorthing` and derives
    // a Horn surface gradient. Fails loudly on a non-elevation layer, a point
    // outside the grid, or any nodata cell; it never invents an orientation.
    static bool fromDem(const RasterLayer &dem, const QPointF &eastingNorthing,
                        SurfaceOrientation *out,
                        QString *errorOut = nullptr);
};
