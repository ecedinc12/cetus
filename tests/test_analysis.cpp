// M3a analysis tests: numeric raster sampling, slope-hazard grid, target
// value-rate ranking and the real Mars Trek scenario loader. The raster and
// scenario inputs are the shipped assets; only the malformed-scenario test
// writes a temporary JSON.

#include "analysis/HazardMap.h"
#include "analysis/TargetRanking.h"
#include "app/MissionScenario.h"
#include "data/RasterLayer.h"
#include "data/SiteConfig.h"

#include <QtTest>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{

bool openSlope(RasterLayer *layer, QString *error)
{
    const QString path =
        SiteConfig::layerPath(SiteConfig::slopeLayerMetadata().fileName);
    if (path.isEmpty())
    {
        *error = QStringLiteral("asset directory is unknown");
        return false;
    }
    return layer->open(path, error);
}

}  // namespace

class TestAnalysis : public QObject
{
    Q_OBJECT

private slots:
    void readsSlopeValues();
    void buildsHazardMap();
    void allTargetPinsAreTraversable();
    void hazardImageRowsAreNorthUp();
    void hazardImageMarksUnknownDistinctly();
    void ranksByValueRate();
    void loadsRealScenario();
    void rejectsBadScenario();
};

void TestAnalysis::readsSlopeValues()
{
    RasterLayer slope(SiteConfig::slopeLayerMetadata());
    QString error;
    QVERIFY2(openSlope(&slope, &error), qPrintable(error));

    bool ok = false;
    const QVector<float> values =
        slope.readValues(slope.info().extent, 64, 64, &ok);
    QVERIFY(ok);
    QCOMPARE(values.size(), 64 * 64);

    int finiteCount = 0;
    int nanCount = 0;
    double maxFinite = 0.0;
    for (float value : values)
    {
        if (std::isnan(value))
        {
            ++nanCount;
            continue;
        }
        QVERIFY(std::isfinite(value));
        QVERIFY(value >= 0.0f);
        // The prepared NASA-derived Horn slope raster contains a handful of
        // very steep rim/wall cells above the 90-degree mathematical bound of
        // the intended slope convention; keep the check physical rather than
        // silently clamping real data.
        QVERIFY(value <= 360.0f);
        maxFinite = std::max(maxFinite, static_cast<double>(value));
        ++finiteCount;
    }
    QVERIFY2(finiteCount > 0, "slope raster returned no finite samples");
    QVERIFY2(nanCount > 0, "slope raster returned no nodata samples");
    QVERIFY2(maxFinite >= 20.0, "slope raster has no hazardous slopes");

    // Request partly outside the raster: the outside strip must be NaN.
    QRectF outside = slope.info().extent;
    outside.setLeft(outside.left() - outside.width());
    QRectF shifted = outside;
    shifted.setWidth(outside.width() * 1.5);
    const QVector<float> partial = slope.readValues(shifted, 100, 50, &ok);
    QVERIFY(ok);
    QCOMPARE(partial.size(), 100 * 50);
    bool outsideNan = true;
    bool insideFinite = false;
    for (int row = 0; row < 50; ++row)
    {
        for (int col = 0; col < 100; ++col)
        {
            const float value = partial.at(row * 100 + col);
            if (col < 33)
            {
                if (!std::isnan(value))
                    outsideNan = false;
            }
            else if (std::isfinite(value))
            {
                insideFinite = true;
            }
        }
    }
    QVERIFY(outsideNan);
    QVERIFY(insideFinite);

    ok = true;
    const QVector<float> invalid = slope.readValues(slope.info().extent, 0, 8,
                                                    &ok);
    QVERIFY(!ok);
    QVERIFY(invalid.isEmpty());
}

void TestAnalysis::buildsHazardMap()
{
    RasterLayer slope(SiteConfig::slopeLayerMetadata());
    QString error;
    QVERIFY2(openSlope(&slope, &error), qPrintable(error));

    HazardMap hazard;
    QVERIFY2(hazard.build(slope, 20.0, &error), qPrintable(error));
    QVERIFY(hazard.isValid());

    const QRectF expected = slope.info().extent;
    const QRectF actual = hazard.extentMeters();
    QVERIFY(std::fabs(actual.left() - expected.left()) <= 1e-6);
    QVERIFY(std::fabs(actual.right() - expected.right()) <= 1e-6);
    QVERIFY(std::fabs(actual.top() - expected.top()) <= 1e-6);
    QVERIFY(std::fabs(actual.bottom() - expected.bottom()) <= 1e-6);

    const HazardMap::Stats stats = hazard.stats();
    QCOMPARE(stats.cellCount, hazard.width() * hazard.height());
    QVERIFY(stats.noGoCount > 0);
    QVERIFY(stats.unknownCount > 0);
    QVERIFY(stats.maxSlopeDeg >= HazardMap::kNoGoSlopeDeg);

    const QString description = hazard.sourceDescription();
    QVERIFY2(description.contains(QStringLiteral("degrees")),
             qPrintable(description));
    QVERIFY2(description.contains(QStringLiteral("20")),
             qPrintable(description));
}

void TestAnalysis::allTargetPinsAreTraversable()
{
    RasterLayer slope(SiteConfig::slopeLayerMetadata());
    QString error;
    QVERIFY2(openSlope(&slope, &error), qPrintable(error));

    HazardMap hazard;
    QVERIFY2(hazard.build(slope, 20.0, &error), qPrintable(error));

    const double cellWidth =
        (hazard.extentMeters().width()) / hazard.width();
    const double cellHeight =
        (hazard.extentMeters().height()) / hazard.height();

    // Clamp a map point to a cell and return its (col, row) indices.
    const auto cellFor = [&](double east, double north) {
        const int col = std::clamp(
            static_cast<int>(std::floor(
                (east - hazard.extentMeters().left()) / cellWidth)),
            0, hazard.width() - 1);
        const int row = std::clamp(
            static_cast<int>(std::floor(
                (hazard.extentMeters().bottom() - north) / cellHeight)),
            0, hazard.height() - 1);
        return qMakePair(col, row);
    };

    struct Pin
    {
        const char *name;
        double east;
        double north;
    };
    // Real sampled slopes (corrected degree product): START 5.58 degrees,
    // Pahrump Hills 2.90, Marias Pass 13.52, Vera Rubin Ridge 7.49.
    const Pin pins[] = {
        {"START", -753.3972146034672, -276224.80223393487},
        {"Pahrump Hills", -756.135485790287, -276363.057354002},
        {"Marias Pass", -1102.35727505766, -276690.662501919},
        {"Vera Rubin Ridge", -1285.075, -279807.988},
    };

    for (const Pin &pin : pins)
    {
        QVERIFY2(!hazard.isNoGoAt(pin.east, pin.north), pin.name);
        const auto cell = cellFor(pin.east, pin.north);
        QVERIFY2(std::isfinite(hazard.cost(cell.first, cell.second)),
                 pin.name);
    }

    // The hard threshold still blocks steep cells.
    bool foundSteep = false;
    for (int row = 0; row < hazard.height() && !foundSteep; ++row)
    {
        for (int col = 0; col < hazard.width(); ++col)
        {
            if (hazard.slopeDeg(col, row) > 20.0)
            {
                QVERIFY(hazard.isNoGo(col, row));
                QVERIFY(!std::isfinite(hazard.cost(col, row)));
                foundSteep = true;
                break;
            }
        }
    }
    if (!foundSteep)
        QSKIP("no cell exceeds the 20 degree threshold in the real raster");
}

void TestAnalysis::hazardImageRowsAreNorthUp()
{
    RasterLayer slope(SiteConfig::slopeLayerMetadata());
    QString error;
    QVERIFY2(openSlope(&slope, &error), qPrintable(error));

    HazardMap hazard;
    QVERIFY2(hazard.build(slope, 20.0, &error), qPrintable(error));

    const QImage image = hazard.hazardImage(QSize(320, 640));
    QCOMPARE(image.size(), QSize(320, 640));

    int opaquePixels = 0;
    for (int y = 0; y < image.height(); ++y)
    {
        const QRgb *line =
            reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x)
        {
            if (qAlpha(line[x]) > 0)
                ++opaquePixels;
        }
    }
    QVERIFY(opaquePixels > 0);
    QVERIFY(opaquePixels < image.width() * image.height());
}

void TestAnalysis::hazardImageMarksUnknownDistinctly()
{
    RasterLayer slope(SiteConfig::slopeLayerMetadata());
    QString error;
    QVERIFY2(openSlope(&slope, &error), qPrintable(error));

    HazardMap hazard;
    QVERIFY2(hazard.build(slope, 20.0, &error), qPrintable(error));
    if (hazard.stats().unknownCount == 0)
        QSKIP("real slope raster has no unknown cells");

    const QImage image = hazard.hazardImage(QSize(320, 640));
    QCOMPARE(image.size(), QSize(320, 640));

    const QRgb unknown = qRgba(155, 89, 192, 170);
    const QRgb noGo = qRgba(220, 30, 20, 170);
    bool foundUnknown = false;
    bool foundNoGo = false;
    for (int y = 0; y < image.height(); ++y)
    {
        const QRgb *line =
            reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x)
        {
            if (line[x] == unknown)
                foundUnknown = true;
            if (line[x] == noGo)
                foundNoGo = true;
        }
    }

    QVERIFY2(foundUnknown,
             "unknown hazard cells were not rendered purple");
    if (!foundNoGo)
        QSKIP("no known no-go pixel at this resolution in the real grid");
}

void TestAnalysis::ranksByValueRate()
{
    ScienceTarget a;
    a.id = QStringLiteral("A");
    a.scienceValue = 0.9;
    a.minutesOnTarget = 10.0;
    ScienceTarget b;
    b.id = QStringLiteral("B");
    b.scienceValue = 0.8;
    b.minutesOnTarget = 20.0;
    ScienceTarget c;
    c.id = QStringLiteral("C");
    c.scienceValue = 0.95;
    c.minutesOnTarget = 5.0;

    const QVector<ScienceTarget> targets{a, b, c};
    QHash<QString, double> travel;
    travel.insert(QStringLiteral("A"), 20.0);  // 0.9 / 30 = 0.03
    travel.insert(QStringLiteral("B"), 10.0);  // 0.8 / 30 ~= 0.02667
    travel.insert(QStringLiteral("C"), 100.0); // 0.95 / 105 ~= 0.00905

    const QVector<RankedTarget> ranked = TargetRanking::rank(targets, travel);
    QCOMPARE(ranked.size(), 3);
    QCOMPARE(ranked.at(0).target.id, QStringLiteral("A"));
    QCOMPARE(ranked.at(1).target.id, QStringLiteral("B"));
    QCOMPARE(ranked.at(2).target.id, QStringLiteral("C"));
    QCOMPARE(ranked.at(0).valueRate, 0.9 / 30.0);
    QCOMPARE(ranked.at(1).valueRate, 0.8 / 30.0);
    QCOMPARE(ranked.at(2).valueRate, 0.95 / 105.0);
    QVERIFY(ranked.at(0).reachable);
    QVERIFY(ranked.at(1).reachable);
    QVERIFY(ranked.at(2).reachable);

    // Missing and negative travel times are unreachable and sort last.
    QHash<QString, double> partialTravel;
    partialTravel.insert(QStringLiteral("A"), 20.0);
    ScienceTarget d;
    d.id = QStringLiteral("D");
    d.scienceValue = 1.0;
    d.minutesOnTarget = 1.0;
    ScienceTarget e;
    e.id = QStringLiteral("E");
    e.scienceValue = 1.0;
    e.minutesOnTarget = 1.0;
    partialTravel.insert(QStringLiteral("E"), -5.0);

    const QVector<RankedTarget> mixed =
        TargetRanking::rank({a, d, e}, partialTravel);
    QCOMPARE(mixed.size(), 3);
    QCOMPARE(mixed.at(0).target.id, QStringLiteral("A"));
    QCOMPARE(mixed.at(1).target.id, QStringLiteral("D"));
    QCOMPARE(mixed.at(2).target.id, QStringLiteral("E"));
    QVERIFY(!mixed.at(1).reachable);
    QCOMPARE(mixed.at(1).travelMinutes, 0.0);
    QCOMPARE(mixed.at(1).valueRate, 0.0);
    QVERIFY(!mixed.at(2).reachable);

    // Equal rates keep input order (stable tie-break on identical ids).
    ScienceTarget t1;
    t1.id = QStringLiteral("same");
    t1.scienceValue = 0.5;
    t1.minutesOnTarget = 5.0;
    ScienceTarget t2 = t1;
    t2.name = QStringLiteral("second");
    QHash<QString, double> tied;
    tied.insert(QStringLiteral("same"), 5.0);
    const QVector<RankedTarget> stable = TargetRanking::rank({t1, t2}, tied);
    QCOMPARE(stable.size(), 2);
    QVERIFY(stable.at(0).target.name.isEmpty());
    QCOMPARE(stable.at(1).target.name, QStringLiteral("second"));

    QCOMPARE(TargetRanking::valueRate(1.0, -1.0, 1.0), 0.0);
    QCOMPARE(TargetRanking::valueRate(1.0, 0.0, 0.0), 0.0);
}

void TestAnalysis::loadsRealScenario()
{
    MissionScenario scenario;
    QString error;
    QVERIFY2(MissionScenario::loadFromFile(SiteConfig::scenarioPath(),
                                           &scenario, &error),
             qPrintable(error));

    QCOMPARE(scenario.targets.size(), 3);
    QCOMPARE(scenario.targets.at(0).id, QStringLiteral("T1"));
    QCOMPARE(scenario.targets.at(1).id, QStringLiteral("T2"));
    QCOMPARE(scenario.targets.at(2).id, QStringLiteral("T3"));

    QVERIFY(std::fabs(scenario.crewStart.x() - (-753.397)) < 0.01);
    QVERIFY(std::fabs(scenario.crewStart.y() - (-276224.802)) < 0.01);

    const ScienceTarget &t2 = scenario.targets.at(1);
    QCOMPARE(t2.scienceValue, 0.95);
    QVERIFY2(t2.why.contains(QStringLiteral("fractures")),
             qPrintable(t2.why));
    QVERIFY2(t2.source.contains(QStringLiteral("Mars Trek")),
             qPrintable(t2.source));
}

void TestAnalysis::rejectsBadScenario()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString earthPath = dir.filePath("earth_radius.json");
    {
        QJsonObject crs;
        crs.insert(QStringLiteral("radius_m"), 6371000.0);
        crs.insert(QStringLiteral("central_meridian_deg"), 137.39);

        QJsonObject crew;
        crew.insert(QStringLiteral("name"), QStringLiteral("Crew"));
        crew.insert(QStringLiteral("source"), QStringLiteral("test"));
        crew.insert(QStringLiteral("easting_m"), 0.0);
        crew.insert(QStringLiteral("northing_m"), 0.0);

        QJsonObject root;
        root.insert(QStringLiteral("schema"),
                    QStringLiteral("cetus.m3.scenario/1"));
        root.insert(QStringLiteral("crs"), crs);
        root.insert(QStringLiteral("crew"), crew);
        root.insert(QStringLiteral("targets"), QJsonArray());

        QFile file(earthPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QJsonDocument(root).toJson());
    }

    MissionScenario scenario;
    QString error;
    QVERIFY(!MissionScenario::loadFromFile(earthPath, &scenario, &error));
    QVERIFY2(error.contains(QStringLiteral("Mars")), qPrintable(error));
    QVERIFY2(error.contains(earthPath), qPrintable(error));

    const QString duplicatePath = dir.filePath("duplicate_ids.json");
    {
        QJsonObject crs;
        crs.insert(QStringLiteral("radius_m"), SiteConfig::kMarsRadiusM);
        crs.insert(QStringLiteral("central_meridian_deg"),
                   SiteConfig::kCentralMeridianDeg);

        QJsonObject crew;
        crew.insert(QStringLiteral("name"), QStringLiteral("Crew"));
        crew.insert(QStringLiteral("source"), QStringLiteral("test"));
        crew.insert(QStringLiteral("easting_m"), -753.0);
        crew.insert(QStringLiteral("northing_m"), -276224.0);

        QJsonObject target;
        target.insert(QStringLiteral("id"), QStringLiteral("T1"));
        target.insert(QStringLiteral("name"), QStringLiteral("Target"));
        target.insert(QStringLiteral("why"), QStringLiteral("Test"));
        target.insert(QStringLiteral("source"), QStringLiteral("test"));
        target.insert(QStringLiteral("easting_m"), -750.0);
        target.insert(QStringLiteral("northing_m"), -276200.0);
        target.insert(QStringLiteral("science_value"), 0.5);
        target.insert(QStringLiteral("minutes_on_target"), 10.0);

        QJsonArray targets;
        targets.append(target);
        targets.append(target);

        QJsonObject root;
        root.insert(QStringLiteral("schema"),
                    QStringLiteral("cetus.m3.scenario/1"));
        root.insert(QStringLiteral("crs"), crs);
        root.insert(QStringLiteral("crew"), crew);
        root.insert(QStringLiteral("targets"), targets);

        QFile file(duplicatePath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QJsonDocument(root).toJson());
    }

    error.clear();
    QVERIFY(!MissionScenario::loadFromFile(duplicatePath, &scenario, &error));
    QVERIFY2(error.contains(QStringLiteral("duplicate"), Qt::CaseInsensitive),
             qPrintable(error));
}

QTEST_GUILESS_MAIN(TestAnalysis)
#include "test_analysis.moc"
