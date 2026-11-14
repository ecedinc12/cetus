#include "app/SampleCollector.h"

#include "analysis/TerrainOrientation.h"
#include "data/RasterLayer.h"
#include "data/SiteConfig.h"
#include "gis/PlanetaryCrs.h"

#include <cmath>
#include <utility>

namespace
{

bool fail(QString *errorOut, const QString &message)
{
    if (errorOut)
        *errorOut = message;
    return false;
}

QString pixelSizeText(double pixelSize)
{
    return QString::number(pixelSize, 'g', 12);
}

}  // namespace

SampleCollector::SampleCollector(const RasterLayer *dem, QString logPath)
    : m_dem(dem)
    , m_log(std::move(logPath))
{
}

bool SampleCollector::load(QString *errorOut)
{
    m_loaded = false;

    if (!m_dem)
        return fail(errorOut,
                    QStringLiteral("DEM is missing; cannot collect a sample "
                                   "without terrain context."));
    if (!m_dem->isValid())
        return fail(errorOut,
                    QStringLiteral("DEM is not open; cannot collect a sample "
                                   "without terrain context."));
    if (m_dem->metadata().kind != RasterLayer::Kind::Elevation)
        return fail(errorOut,
                    QStringLiteral("DEM layer '%1' is not elevation data; "
                                   "cannot collect a sample.")
                        .arg(m_dem->metadata().id));

    if (m_log.filePath().isEmpty())
        return fail(errorOut,
                    QStringLiteral("sample log path is empty; set "
                                   "CETUS_SAMPLE_LOG or provide an app-data "
                                   "location."));

    if (!m_log.load(errorOut))
        return false;

    m_loaded = true;
    if (errorOut)
        errorOut->clear();
    return true;
}

bool SampleCollector::isReady() const
{
    return m_loaded && m_dem && m_dem->isValid();
}

const SampleLog &SampleCollector::log() const
{
    return m_log;
}

QString SampleCollector::logPath() const
{
    return m_log.filePath();
}

bool SampleCollector::collect(const ScienceTarget &target, int sol,
                              const QDateTime &timestampUtc,
                              SampleRecord *out, QString *errorOut)
{
    if (!out)
        return fail(errorOut, QStringLiteral("no sample record output"));
    if (!m_loaded)
        return fail(errorOut,
                    QStringLiteral("sample collector is not loaded; cannot "
                                   "collect for target '%1'.")
                        .arg(target.id));
    if (sol < 0)
        return fail(errorOut,
                    QStringLiteral("target '%1': sol %2 is invalid")
                        .arg(target.id)
                        .arg(sol));
    if (!timestampUtc.isValid())
        return fail(errorOut,
                    QStringLiteral("target '%1': timestamp is invalid")
                        .arg(target.id));
    if (target.phenomenon.trimmed().isEmpty())
        return fail(errorOut,
                    QStringLiteral("target '%1' has no 'phenomenon'; refusing "
                                   "to write a field note without it.")
                        .arg(target.id));

    SurfaceOrientation orientation;
    QString orientationError;
    if (!TerrainOrientation::fromDem(
            *m_dem, QPointF(target.eastingM, target.northingM), &orientation,
            &orientationError))
        return fail(errorOut,
                    QStringLiteral("target '%1': %2")
                        .arg(target.id, orientationError));

    const PlanetaryCrs::LonLat lonLat = PlanetaryCrs::eqcToLonLat(
        target.eastingM, target.northingM, SiteConfig::kCentralMeridianDeg);

    const RasterLayer::Metadata &metadata = m_dem->metadata();
    const QString pixelSize = pixelSizeText(m_dem->info().pixelSizeX);

    SampleRecord record;
    record.sol = sol;
    record.timestampUtc = timestampUtc.toUTC();

    record.targetId = target.id;
    record.targetName = target.name;
    record.targetSource = target.source;
    record.eastingM = target.eastingM;
    record.northingM = target.northingM;
    record.lonDeg = lonLat.lonDeg;
    record.latDeg = lonLat.latDeg;

    record.upEast = orientation.upEast;
    record.upNorth = orientation.upNorth;
    record.upUp = orientation.upUp;
    record.slopeDeg = orientation.slopeDeg;
    record.aspectDeg = orientation.aspectDeg;
    record.aspectDefined = orientation.aspectDefined;
    record.orientationMethod = orientation.method;
    record.orientationSource =
        QStringLiteral("%1 \u00b7 %2 at %3 m/px")
            .arg(metadata.source, metadata.product, pixelSize);

    record.phenomenon = target.phenomenon;
    record.why = target.why;
    record.positionSource =
        QStringLiteral("%1 target pin transformed to the site Mars "
                       "equirectangular CRS")
            .arg(target.source);

    const QString aspectText =
        orientation.aspectDefined
            ? QStringLiteral("downslope aspect %1\u00b0 clockwise from north")
                  .arg(orientation.aspectDeg, 0, 'f', 1)
            : QStringLiteral("aspect not defined (locally flat)");
    record.measurement =
        QStringLiteral("Terrain context only: Horn 3x3 slope %1\u00b0, %2. "
                       "No in-situ composition measurement was recorded.")
            .arg(orientation.slopeDeg, 0, 'f', 2)
            .arg(aspectText);
    record.compositionMeasured = false;

    if (!m_log.append(&record, errorOut))
        return false;

    *out = record;
    if (errorOut)
        errorOut->clear();
    return true;
}
