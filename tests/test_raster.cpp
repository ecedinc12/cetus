// M1a raster core tests: SiteConfig constants, RasterLayer open/render
// behaviour on synthetic GDAL fixtures, and the prepared-asset registry.
// Fixture rasters are written with the GDAL C++ API into a QTemporaryDir;
// the "real assets" test runs only when the prepared NASA rasters exist.

#include "data/DatasetRegistry.h"
#include "data/RasterLayer.h"
#include "data/SiteConfig.h"
#include "gis/PlanetaryCrs.h"

#include <gdal_priv.h>
#include <ogr_spatialref.h>

#include <QtTest>

#include <QFileInfo>
#include <QTemporaryDir>

#include <cmath>
#include <string>

namespace
{

// Writes a single-band north-up GeoTIFF. When epsgCode > 0 the raster gets
// that Earth EPSG CRS, otherwise the Mars site CRS from SiteConfig.
bool createFixtureRaster(const QString &path, const double gt[6], int width,
                         int height, GDALDataType type, const void *values,
                         int epsgCode = 0, bool setNoData = false,
                         double noData = 0.0)
{
    GDALAllRegister();
    GDALDriver *driver = GetGDALDriverManager()->GetDriverByName("GTiff");
    if (!driver)
        return false;

    GDALDataset *ds =
        driver->Create(path.toUtf8().constData(), width, height, 1, type,
                       nullptr);
    if (!ds)
        return false;

    bool ok = ds->SetGeoTransform(gt) == CE_None;

    OGRSpatialReference srs;
    if (epsgCode > 0)
        ok = ok && srs.importFromEPSG(epsgCode) == OGRERR_NONE;
    else
        ok = ok &&
             srs.importFromProj4(SiteConfig::kSiteCrsProj4) == OGRERR_NONE;
    if (ok)
    {
        const std::string wkt = srs.exportToWkt();
        ok = !wkt.empty() && ds->SetProjection(wkt.c_str()) == CE_None;
    }

    GDALRasterBand *band = ds->GetRasterBand(1);
    if (setNoData)
        ok = band->SetNoDataValue(noData) == CE_None && ok;
    ok = band->RasterIO(GF_Write, 0, 0, width, height,
                        const_cast<void *>(values), width, height, type, 0,
                        0) == CE_None &&
         ok;

    GDALClose(ds);
    return ok;
}

// QRectF bounds normalised: x grows east, y grows north (extent.bottom()
// is the north edge of a north-up grid).
double minX(const QRectF &r)
{
    return std::min(r.left(), r.right());
}
double maxX(const QRectF &r)
{
    return std::max(r.left(), r.right());
}
double minY(const QRectF &r)
{
    return std::min(r.top(), r.bottom());
}
double maxY(const QRectF &r)
{
    return std::max(r.top(), r.bottom());
}

bool containsPoint(const QRectF &r, const QPointF &p)
{
    return p.x() >= minX(r) && p.x() <= maxX(r) && p.y() >= minY(r) &&
           p.y() <= maxY(r);
}

bool rectsIntersect(const QRectF &a, const QRectF &b)
{
    return minX(a) < maxX(b) && minX(b) < maxX(a) && minY(a) < maxY(b) &&
           minY(b) < maxY(a);
}

}  // namespace

class TestRaster : public QObject
{
    Q_OBJECT

private slots:
    void siteConstantsMatchFixedFacts();
    void byteRasterNorthUp();
    void earthCrsRejected();
    void floatRasterNodataTransparent();
    void extentLargerThanRasterClips();
    void rotatedGeotransformRejected();
    void realAssetsLoad();
};

void TestRaster::siteConstantsMatchFixedFacts()
{
    QCOMPARE(SiteConfig::kMarsRadiusM, 3396190.0);
    QCOMPARE(SiteConfig::kCentralMeridianDeg, 137.39);
    QCOMPARE(SiteConfig::kAnchorLatDeg, -4.695);
    QCOMPARE(SiteConfig::kAnchorLonDeg, 137.381);
    QCOMPARE(QString::fromLatin1(SiteConfig::kSiteCrsProj4),
             PlanetaryCrs::marsEquirectangular(SiteConfig::kCentralMeridianDeg));
    QCOMPARE(QString::fromUtf8(SiteConfig::kSiteName),
             QStringLiteral("Gale Crater \u2014 Pahrump Hills / Garden City"));

    // Asset dir: non-empty env wins, else the CMake macro; never the CWD.
    QVERIFY(!SiteConfig::assetDir().isEmpty());

    const QByteArray savedEnv = qgetenv("CETUS_ASSET_DIR");
    qputenv("CETUS_ASSET_DIR", QByteArray("/tmp/cetus_fake_assets"));
    QCOMPARE(SiteConfig::assetDir(), QStringLiteral("/tmp/cetus_fake_assets"));
    QCOMPARE(SiteConfig::layerPath(QStringLiteral("a.tif")),
             QStringLiteral("/tmp/cetus_fake_assets/layers/a.tif"));

    qunsetenv("CETUS_ASSET_DIR");
    QCOMPARE(SiteConfig::assetDir(), QStringLiteral(CETUS_ASSET_DIR));

    // Restore for the remaining slots (real-assets test needs the env or
    // the macro; both point at the same assets dir here).
    if (savedEnv.isEmpty())
        qunsetenv("CETUS_ASSET_DIR");
    else
        qputenv("CETUS_ASSET_DIR", savedEnv);
}

void TestRaster::byteRasterNorthUp()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("byte_northup.tif");

    // gt = [0, 1, 0, 2, 0, -1]: origin (0, 2), 1 m pixels, north-up.
    const double gt[6] = {0.0, 1.0, 0.0, 2.0, 0.0, -1.0};
    // Row 0 (north) = 10 20, row 1 (south) = 30 40.
    const GByte values[4] = {10, 20, 30, 40};
    QVERIFY(createFixtureRaster(path, gt, 2, 2, GDT_Byte, values));

    RasterLayer raster(SiteConfig::opticalLayerMetadata());
    QString error;
    QVERIFY2(raster.open(path, &error), qPrintable(error));
    QVERIFY(raster.isValid());

    const RasterLayer::Info &info = raster.info();
    QVERIFY(info.isMars);
    QVERIFY(std::abs(info.semiMajorAxisM - 3396190.0) < 1.0);
    QCOMPARE(info.width, 2);
    QCOMPARE(info.height, 2);
    QCOMPARE(info.pixelSizeX, 1.0);
    QCOMPARE(info.pixelSizeY, 1.0);
    QCOMPARE(info.bandCount, 1);
    QVERIFY(!info.crsProj4.isEmpty());

    const QImage image = raster.readImage(QRectF(0.0, 0.0, 2.0, 2.0),
                                          QSize(2, 2));
    QCOMPARE(image.size(), QSize(2, 2));
    // North row first: rows are NOT flipped.
    QCOMPARE(qRed(image.pixel(0, 0)), 10);
    QCOMPARE(qRed(image.pixel(1, 0)), 20);
    QCOMPARE(qRed(image.pixel(0, 1)), 30);
    QCOMPARE(qRed(image.pixel(1, 1)), 40);
    QCOMPARE(qAlpha(image.pixel(0, 0)), 255);
    QCOMPARE(qRed(image.pixel(0, 0)), qGreen(image.pixel(0, 0)));
    QCOMPARE(qRed(image.pixel(0, 0)), qBlue(image.pixel(0, 0)));
}

void TestRaster::earthCrsRejected()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("earth_3857.tif");

    const double gt[6] = {0.0, 1.0, 0.0, 2.0, 0.0, -1.0};
    const GByte values[4] = {10, 20, 30, 40};
    QVERIFY(createFixtureRaster(path, gt, 2, 2, GDT_Byte, values, 3857));

    RasterLayer raster(SiteConfig::opticalLayerMetadata());
    QString error;
    QVERIFY(!raster.open(path, &error));
    QVERIFY(!raster.isValid());
    QVERIFY2(error.contains(QStringLiteral("Mars"), Qt::CaseInsensitive),
             qPrintable(error));
    QVERIFY2(error.contains(QStringLiteral("Earth"), Qt::CaseInsensitive),
             qPrintable(error));
}

void TestRaster::floatRasterNodataTransparent()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("float_nodata.tif");

    const double gt[6] = {0.0, 1.0, 0.0, 2.0, 0.0, -1.0};
    // Nodata in the north-west pixel, valid values elsewhere.
    const float values[4] = {-9999.0f, 100.0f, 200.0f, 300.0f};
    QVERIFY(createFixtureRaster(path, gt, 2, 2, GDT_Float32, values, 0, true,
                                -9999.0));

    RasterLayer raster(SiteConfig::slopeLayerMetadata());
    QString error;
    QVERIFY2(raster.open(path, &error), qPrintable(error));
    QVERIFY(raster.info().hasNoData);
    QCOMPARE(raster.info().noDataValue, -9999.0);

    const QImage image = raster.readImage(QRectF(0.0, 0.0, 2.0, 2.0),
                                          QSize(2, 2));
    QCOMPARE(image.size(), QSize(2, 2));
    QCOMPARE(qAlpha(image.pixel(0, 0)), 0);  // nodata -> transparent
    QCOMPARE(qAlpha(image.pixel(1, 0)), 255);
    QCOMPARE(qAlpha(image.pixel(0, 1)), 255);
    QCOMPARE(qAlpha(image.pixel(1, 1)), 255);
}

void TestRaster::extentLargerThanRasterClips()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("clip_me.tif");

    // 2x2 raster covering [0, 2] x [0, 2] m.
    const double gt[6] = {0.0, 1.0, 0.0, 2.0, 0.0, -1.0};
    const GByte values[4] = {10, 20, 30, 40};
    QVERIFY(createFixtureRaster(path, gt, 2, 2, GDT_Byte, values));

    RasterLayer raster(SiteConfig::opticalLayerMetadata());
    QString error;
    QVERIFY2(raster.open(path, &error), qPrintable(error));

    // Request twice the size in every direction: margins transparent,
    // covered centre opaque.
    const QImage image = raster.readImage(QRectF(-1.0, -1.0, 4.0, 4.0),
                                          QSize(4, 4));
    QCOMPARE(image.size(), QSize(4, 4));
    QCOMPARE(qAlpha(image.pixel(0, 0)), 0);   // margin (outside raster)
    QCOMPARE(qAlpha(image.pixel(3, 3)), 0);   // margin (outside raster)
    QCOMPARE(qAlpha(image.pixel(2, 2)), 255); // covered centre
    // Centre pixel maps to the raster's south-east value.
    QCOMPARE(qRed(image.pixel(2, 2)), 40);
}

void TestRaster::rotatedGeotransformRejected()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("rotated.tif");

    // Non-zero rotation term (gt[2]): must fail loudly, never flip silently.
    const double gt[6] = {0.0, 1.0, 0.05, 2.0, 0.0, -1.0};
    const GByte values[4] = {10, 20, 30, 40};
    QVERIFY(createFixtureRaster(path, gt, 2, 2, GDT_Byte, values));

    RasterLayer raster(SiteConfig::opticalLayerMetadata());
    QString error;
    QVERIFY(!raster.open(path, &error));
    QVERIFY2(error.contains(QStringLiteral("north-up"), Qt::CaseInsensitive),
             qPrintable(error));
}

void TestRaster::realAssetsLoad()
{
    const QString orthoPath =
        SiteConfig::layerPath(QStringLiteral("gale_hirise_ortho_1m.tif"));
    if (orthoPath.isEmpty() || !QFileInfo::exists(orthoPath))
        QSKIP("prepared assets absent (assets/layers/gale_hirise_ortho_1m.tif "
              "not found)");

    DatasetRegistry registry;
    QString error;
    QVERIFY2(registry.loadPrepared(&error), qPrintable(error));

    const QStringList expectedIds{QStringLiteral("hirise_hillshade"),
                                  QStringLiteral("hirise_dem"),
                                  QStringLiteral("hirise_slope"),
                                  QStringLiteral("hirise_optical")};
    QCOMPARE(registry.ids(), expectedIds);
    QCOMPARE(registry.layers().size(), std::size_t(4));

    for (const auto &layer : registry.layers())
    {
        const QString id = layer->metadata().id;
        QVERIFY2(layer->info().isMars, qPrintable(id));
        QVERIFY2(std::abs(layer->info().semiMajorAxisM - 3396190.0) < 1.0,
                 qPrintable(id));
        QVERIFY2(!layer->info().crsProj4.isEmpty(), qPrintable(id));
        QVERIFY(layer->info().width > 0);
        QVERIFY(layer->info().height > 0);
    }

    const RasterLayer *ortho = registry.layer(QStringLiteral("hirise_optical"));
    QVERIFY(ortho != nullptr);
    QCOMPARE(registry.defaultLayer(), ortho);
    QVERIFY(registry.layer(QStringLiteral("hirise_missing")) == nullptr);

    // Brief: ortho pixel size ~1.0118 m/px.
    QVERIFY2(std::abs(ortho->info().pixelSizeX - 1.0118) < 0.001,
             qPrintable(QString::number(ortho->info().pixelSizeX, 'f', 6)));

    const RasterLayer *dem = registry.layer(QStringLiteral("hirise_dem"));
    QVERIFY(dem != nullptr);

    // Ortho and DEM cover the same site: extents must overlap ...
    QVERIFY(rectsIntersect(ortho->info().extent, dem->info().extent));

    // ... and the mission anchor lies inside both extents.
    const QPointF anchor =
        PlanetaryCrs::lonLatToEqc(SiteConfig::kAnchorLonDeg,
                                  SiteConfig::kAnchorLatDeg,
                                  SiteConfig::kCentralMeridianDeg);
    QVERIFY2(containsPoint(ortho->info().extent, anchor),
             "anchor outside ortho extent");
    QVERIFY2(containsPoint(dem->info().extent, anchor),
             "anchor outside DEM extent");
}

QTEST_GUILESS_MAIN(TestRaster)
#include "test_raster.moc"
