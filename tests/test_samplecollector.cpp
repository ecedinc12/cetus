// M6 sample collector tests against the shipped HiRISE DEM and Mars Trek
// scenario. Terrain orientation is context only; no composition is claimed.

#include "analysis/TargetRanking.h"
#include "app/MissionScenario.h"
#include "app/SampleCollector.h"
#include "app/SampleLog.h"
#include "data/RasterLayer.h"
#include "data/SiteConfig.h"

#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest/QTest>

#include <cmath>

namespace
{

bool hasRealAssets()
{
    const QString dir = qEnvironmentVariable("CETUS_ASSET_DIR");
    if (dir.isEmpty())
        return false;
    return QFile::exists(dir +
                         QStringLiteral("/layers/gale_hirise_dem_4m.tif"));
}

int lineCount(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return -1;
    int count = 0;
    while (!file.atEnd()) {
        if (!file.readLine().trimmed().isEmpty())
            ++count;
    }
    return count;
}

bool loadTargetT1(MissionScenario *scenario, ScienceTarget *target,
                  double *lonDeg, double *latDeg, QString *error)
{
    const QString path = SiteConfig::scenarioPath();
    if (!MissionScenario::loadFromFile(path, scenario, error))
        return false;

    bool found = false;
    for (const ScienceTarget &candidate : scenario->targets) {
        if (candidate.id == QStringLiteral("T1")) {
            *target = candidate;
            found = true;
            break;
        }
    }
    if (!found) {
        *error = QStringLiteral("scenario has no target T1");
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("cannot open scenario JSON");
        return false;
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    const QJsonArray targets =
        document.object().value(QStringLiteral("targets")).toArray();
    for (const QJsonValue &value : targets) {
        const QJsonObject object = value.toObject();
        if (object.value(QStringLiteral("id")).toString() !=
            QStringLiteral("T1"))
            continue;
        *lonDeg = object.value(QStringLiteral("lon_deg")).toDouble();
        *latDeg = object.value(QStringLiteral("lat_deg")).toDouble();
        return true;
    }
    *error = QStringLiteral("scenario JSON has no T1 lon/lat");
    return false;
}

}  // namespace

class TestSampleCollector : public QObject
{
    Q_OBJECT

private slots:
    void collectsRealTargetWithTerrainContext();
    void missingDemFailsLoudly();
    void missingPhenomenonFailsLoudly();
};

void TestSampleCollector::collectsRealTargetWithTerrainContext()
{
    if (!hasRealAssets())
        QSKIP("prepared HiRISE DEM not present");

    MissionScenario scenario;
    ScienceTarget target;
    double lonDeg = 0.0;
    double latDeg = 0.0;
    QString error;
    QVERIFY2(loadTargetT1(&scenario, &target, &lonDeg, &latDeg, &error),
             qPrintable(error));

    RasterLayer dem(SiteConfig::demLayerMetadata());
    QVERIFY2(dem.open(SiteConfig::layerPath(dem.metadata().fileName),
                      &error),
             qPrintable(error));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("samples.jsonl"));

    SampleCollector collector(&dem, path);
    QVERIFY2(collector.load(&error), qPrintable(error));
    QVERIFY(collector.isReady());

    const QDateTime timestamp(QDate(2026, 11, 14), QTime(22, 15, 0), QTimeZone::UTC);
    SampleRecord record;
    QVERIFY2(collector.collect(target, 3, timestamp, &record, &error),
             qPrintable(error));

    QCOMPARE(record.id, QStringLiteral("SAMPLE-3-1"));
    QCOMPARE(record.targetId, target.id);
    QCOMPARE(record.targetName, target.name);
    QCOMPARE(record.eastingM, target.eastingM);
    QCOMPARE(record.northingM, target.northingM);
    QVERIFY(std::fabs(record.lonDeg - lonDeg) < 1e-6);
    QVERIFY(std::fabs(record.latDeg - latDeg) < 1e-6);

    const double normalLength = std::hypot(
        std::hypot(record.upEast, record.upNorth), record.upUp);
    QVERIFY(std::fabs(normalLength - 1.0) < 1e-6);
    QVERIFY(std::fabs(record.slopeDeg - 2.90) <= 1.0);
    QVERIFY(record.phenomenon.contains(QStringLiteral("Jarosite")));
    QCOMPARE(record.compositionMeasured, false);
    QVERIFY(record.measurement.contains(
        QStringLiteral("No in-situ composition measurement")));
    QVERIFY(!record.orientationSource.isEmpty());
    QVERIFY(!record.positionSource.isEmpty());

    QCOMPARE(lineCount(path), 1);
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray line = file.readLine().trimmed();
    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(line, &parseError);
    QCOMPARE(parseError.error, QJsonParseError::NoError);
    QVERIFY(document.isObject());
    QCOMPARE(document.object().value(QStringLiteral("id")).toString(),
             QStringLiteral("SAMPLE-3-1"));
}

void TestSampleCollector::missingDemFailsLoudly()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("samples.jsonl"));

    SampleCollector collector(nullptr, path);
    QString error;
    QVERIFY(!collector.load(&error));
    QVERIFY2(error.contains(QStringLiteral("DEM")), qPrintable(error));
    QVERIFY(!collector.isReady());
}

void TestSampleCollector::missingPhenomenonFailsLoudly()
{
    if (!hasRealAssets())
        QSKIP("prepared HiRISE DEM not present");

    MissionScenario scenario;
    ScienceTarget target;
    double lonDeg = 0.0;
    double latDeg = 0.0;
    QString error;
    QVERIFY2(loadTargetT1(&scenario, &target, &lonDeg, &latDeg, &error),
             qPrintable(error));

    RasterLayer dem(SiteConfig::demLayerMetadata());
    QVERIFY2(dem.open(SiteConfig::layerPath(dem.metadata().fileName),
                      &error),
             qPrintable(error));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("samples.jsonl"));

    SampleCollector collector(&dem, path);
    QVERIFY2(collector.load(&error), qPrintable(error));

    const QDateTime timestamp(QDate(2026, 11, 14), QTime(22, 15, 0), QTimeZone::UTC);
    SampleRecord record;
    QVERIFY2(collector.collect(target, 3, timestamp, &record, &error),
             qPrintable(error));
    const int linesBefore = lineCount(path);
    const qint64 sizeBefore = QFile(path).size();

    ScienceTarget noPhenomenon = target;
    noPhenomenon.phenomenon.clear();
    SampleRecord rejected;
    QVERIFY(!collector.collect(noPhenomenon, 3, timestamp, &rejected, &error));
    QVERIFY2(error.contains(QStringLiteral("phenomenon")), qPrintable(error));
    QCOMPARE(lineCount(path), linesBefore);
    QCOMPARE(QFile(path).size(), sizeBefore);
}

QTEST_MAIN(TestSampleCollector)
#include "test_samplecollector.moc"
