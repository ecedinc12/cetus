#include "data/SiteConfig.h"

namespace SiteConfig
{

QString assetDir()
{
    // 1) Environment override (tests / packaged layouts).
    const QByteArray env = qgetenv("CETUS_ASSET_DIR");
    if (!env.isEmpty())
        return QString::fromUtf8(env);

    // 2) Compile-time location of the shipped assets directory.
#ifdef CETUS_ASSET_DIR
    return QStringLiteral(CETUS_ASSET_DIR);
#else
    // 3) No fallback (never the CWD): callers must fail loudly.
    return QString();
#endif
}

QString layerPath(const QString &fileName)
{
    const QString dir = assetDir();
    if (dir.isEmpty())
        return QString();
    // Prepared rasters ship in assets/layers/ while CETUS_ASSET_DIR names
    // the assets/ root, so the layers/ segment is inserted here.
    if (dir.endsWith(QLatin1Char('/')))
        return dir + QStringLiteral("layers/") + fileName;
    return dir + QStringLiteral("/layers/") + fileName;
}

QString scenarioPath()
{
    const QString dir = assetDir();
    if (dir.isEmpty())
        return QString();
    if (dir.endsWith(QLatin1Char('/')))
        return dir + QStringLiteral("scenario/m3_scenario.json");
    return dir + QStringLiteral("/scenario/m3_scenario.json");
}

std::vector<RasterLayer::Metadata> preparedLayerMetadata()
{
    // Display order: hillshade, dem, slope, optical (top layer first).
    return {hillshadeLayerMetadata(), demLayerMetadata(),
            slopeLayerMetadata(),     opticalLayerMetadata()};
}

RasterLayer::Metadata hillshadeLayerMetadata()
{
    RasterLayer::Metadata m;
    m.id = QStringLiteral("hirise_hillshade");
    m.title = QStringLiteral("HiRISE Hillshade");
    m.source = QStringLiteral("HiRISE DTM / MRO");
    m.product = QStringLiteral("DTEEC_040770_1755_039280_1755_U01");
    m.resolutionText = QStringLiteral("4 m/px");
    m.units = QStringLiteral("unitless (0\u2013255)");
    m.fileName = QStringLiteral("gale_hirise_hillshade_4m.tif");
    m.kind = RasterLayer::Kind::Hillshade;
    m.ramp = RasterLayer::Ramp::Grayscale;
    m.displayMin = 1.0;
    m.displayMax = 255.0;
    return m;
}

RasterLayer::Metadata demLayerMetadata()
{
    RasterLayer::Metadata m;
    m.id = QStringLiteral("hirise_dem");
    m.title = QStringLiteral("HiRISE DEM");
    m.source = QStringLiteral("HiRISE DTM / MRO");
    m.product = QStringLiteral("DTEEC_040770_1755_039280_1755_U01");
    m.resolutionText = QStringLiteral("4 m/px (native 1 m)");
    m.units = QStringLiteral("m above Mars areoid");
    m.fileName = QStringLiteral("gale_hirise_dem_4m.tif");
    m.kind = RasterLayer::Kind::Elevation;
    m.ramp = RasterLayer::Ramp::Terrain;
    m.displayMin = -4504.0;
    m.displayMax = -3473.0;
    return m;
}

RasterLayer::Metadata slopeLayerMetadata()
{
    RasterLayer::Metadata m;
    m.id = QStringLiteral("hirise_slope");
    m.title = QStringLiteral("HiRISE Slope");
    m.source = QStringLiteral("HiRISE DTM / MRO");
    m.product = QStringLiteral("DTEEC_040770_1755_039280_1755_U01");
    m.resolutionText = QStringLiteral("4 m/px");
    m.units = QStringLiteral("degrees");
    m.fileName = QStringLiteral("gale_hirise_slope_4m.tif");
    m.kind = RasterLayer::Kind::Slope;
    m.ramp = RasterLayer::Ramp::Hazard;
    m.displayMin = 0.0;
    m.displayMax = 40.0;
    return m;
}

RasterLayer::Metadata opticalLayerMetadata()
{
    RasterLayer::Metadata m;
    m.id = QStringLiteral("hirise_optical");
    m.title = QStringLiteral("HiRISE Ortho");
    m.source = QStringLiteral("HiRISE / MRO");
    m.product = QStringLiteral("ESP_040770_1755_RED_C_01_ORTHO");
    m.resolutionText = QStringLiteral("1.01 m/px");
    m.units = QStringLiteral("8-bit reflectance DN");
    m.fileName = QStringLiteral("gale_hirise_ortho_1m.tif");
    m.kind = RasterLayer::Kind::Optical;
    m.ramp = RasterLayer::Ramp::Grayscale;
    m.displayMin = 0.0;
    m.displayMax = 255.0;
    return m;
}

}  // namespace SiteConfig
