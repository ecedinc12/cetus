#include "analysis/TerrainOrientation.h"

#include <QtMath>

#include <cmath>

namespace
{

constexpr double kDegreesPerRadian = 180.0 / 3.14159265358979323846;

bool fail(QString *errorOut, const QString &message)
{
    if (errorOut)
        *errorOut = message;
    return false;
}

}  // namespace

bool TerrainOrientation::fromDem(const RasterLayer &dem,
                                 const QPointF &eastingNorthing,
                                 SurfaceOrientation *out, QString *errorOut)
{
    if (!out)
        return fail(errorOut, QStringLiteral("no orientation output provided"));

    if (!dem.isValid())
        return fail(errorOut,
                    QStringLiteral("DEM is not open; cannot derive an "
                                   "elevation orientation."));

    if (dem.metadata().kind != RasterLayer::Kind::Elevation)
        return fail(errorOut,
                    QStringLiteral("raster '%1' is not an elevation DEM; "
                                   "terrain orientation requires an elevation "
                                   "layer.")
                        .arg(dem.metadata().id));

    const RasterLayer::Info &info = dem.info();
    if (info.width <= 0 || info.height <= 0 || info.pixelSizeX <= 0.0 ||
        info.pixelSizeY <= 0.0)
        return fail(errorOut,
                    QStringLiteral("DEM has an invalid elevation grid; refusing "
                                   "to invent an orientation."));

    const double minX = qMin(info.extent.left(), info.extent.right());
    const double maxX = qMax(info.extent.left(), info.extent.right());
    const double minY = qMin(info.extent.top(), info.extent.bottom());
    const double maxY = qMax(info.extent.top(), info.extent.bottom());

    const double x = eastingNorthing.x();
    const double y = eastingNorthing.y();
    if (!std::isfinite(x) || !std::isfinite(y) || x < minX || x >= maxX ||
        y < minY || y >= maxY)
        return fail(errorOut,
                    QStringLiteral("DEM has no data at the sample position; "
                                   "refusing to invent an orientation."));

    const int col =
        static_cast<int>(std::floor((x - minX) / info.pixelSizeX));
    // Row 0 is the north edge (the raster API is north-up).
    const int row =
        static_cast<int>(std::floor((maxY - y) / info.pixelSizeY));
    if (col < 0 || col >= info.width || row < 0 || row >= info.height)
        return fail(errorOut,
                    QStringLiteral("DEM has no data at the sample position; "
                                   "refusing to invent an orientation."));

    // Exact source-pixel rectangle for the 3x3 neighbourhood centred on the
    // target cell (row 0 = north edge).
    const double west = minX + (col - 1) * info.pixelSizeX;
    const double south = maxY - (row + 2) * info.pixelSizeY;
    const QRectF neighbourhood(west, south, 3.0 * info.pixelSizeX,
                               3.0 * info.pixelSizeY);

    bool ok = false;
    const QVector<float> values =
        dem.readValues(neighbourhood, 3, 3, &ok);
    if (!ok || values.size() != 9)
        return fail(errorOut,
                    QStringLiteral("DEM has no data at the sample position; "
                                   "refusing to invent an orientation."));

    for (float value : values)
    {
        if (std::isnan(value))
            return fail(errorOut,
                        QStringLiteral("DEM has no data at the sample position; "
                                       "refusing to invent an orientation."));
    }

    const double z0 = values.at(0), z1 = values.at(1), z2 = values.at(2);
    const double z3 = values.at(3), z4 = values.at(4), z5 = values.at(5);
    const double z6 = values.at(6), z7 = values.at(7), z8 = values.at(8);
    Q_UNUSED(z4);

    const double denominatorX = 8.0 * info.pixelSizeX;
    const double denominatorY = 8.0 * info.pixelSizeY;
    // Horn 3x3 gradient (gdaldem convention), converted to ENU with dzdy
    // positive north.
    const double dzdx =
        ((z2 + 2.0 * z5 + z8) - (z0 + 2.0 * z3 + z6)) / denominatorX;
    const double dzdy =
        ((z0 + 2.0 * z1 + z2) - (z6 + 2.0 * z7 + z8)) / denominatorY;

    const double nx = -dzdx;
    const double ny = -dzdy;
    const double nz = 1.0;
    const double length = std::hypot(std::hypot(nx, ny), nz);

    SurfaceOrientation orientation;
    orientation.upEast = nx / length;
    orientation.upNorth = ny / length;
    orientation.upUp = nz / length;
    orientation.slopeDeg = std::atan(std::hypot(dzdx, dzdy)) *
                           kDegreesPerRadian;

    const double horizontal = std::hypot(dzdx, dzdy);
    if (horizontal < 1e-6)
    {
        orientation.aspectDefined = false;
        orientation.aspectDeg = 0.0;
    }
    else
    {
        orientation.aspectDefined = true;
        double aspect = std::atan2(-dzdx, -dzdy) * kDegreesPerRadian;
        if (aspect < 0.0)
            aspect += 360.0;
        aspect = std::fmod(aspect, 360.0);
        if (aspect < 0.0)
            aspect += 360.0;
        orientation.aspectDeg = aspect;
    }

    orientation.method =
        QStringLiteral("Horn 3x3 surface gradient on %1 m/px DEM")
            .arg(QString::number(info.pixelSizeX, 'g', 12));

    *out = orientation;
    return true;
}
