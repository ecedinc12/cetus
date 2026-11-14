// M6 sample log tests: per-sol ids, strict JSONL append/load, validation and
// loud corruption handling. All files are written to a temporary directory.

#include "app/SampleLog.h"

#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest/QTest>

#include <cmath>
#include <limits>

namespace
{

SampleRecord makeRecord(int sol, const QString &targetId)
{
    SampleRecord record;
    record.sol = sol;
    record.timestampUtc =
        QDateTime(QDate(2026, 11, 14), QTime(22, 15, 0), QTimeZone::UTC);
    record.targetId = targetId;
    record.targetName = QStringLiteral("Pahrump Hills");
    record.targetSource = QStringLiteral(
        "NASA Mars Trek curiosity_waypoints FID 5 (Pahrump Hills)");
    record.eastingM = -756.135485790287;
    record.northingM = -276363.057354002;
    record.lonDeg = 137.37724353716703;
    record.latDeg = -4.66241193815176;
    record.upEast = 0.0;
    record.upNorth = 0.0;
    record.upUp = 1.0;
    record.slopeDeg = 2.9;
    record.aspectDeg = 0.0;
    record.aspectDefined = false;
    record.orientationMethod =
        QStringLiteral("Horn 3x3 surface gradient on 4 m/px DEM");
    record.orientationSource = QStringLiteral(
        "HiRISE DTM / MRO \u00b7 DTEEC_040770_1755_039280_1755_U01 at 4 m/px");
    record.phenomenon =
        QStringLiteral("Jarosite-bearing fracture fills in mudstone.");
    record.why = QStringLiteral(
        "Records early acidic fluid chemistry at the Mojave drill site.");
    record.measurement = QStringLiteral(
        "Terrain context only: Horn 3x3 slope 2.90\u00b0, aspect not defined "
        "(locally flat). No in-situ composition measurement was recorded.");
    record.positionSource = QStringLiteral(
        "NASA Mars Trek target pin transformed to the site Mars "
        "equirectangular CRS");
    record.compositionMeasured = false;
    return record;
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

}  // namespace

class TestSampleLog : public QObject
{
    Q_OBJECT

private slots:
    void appendsJsonlAndContinuesPerSol();
    void rejectsInvalidRecord();
    void corruptLogFailsLoudly();
    void defaultPathHonoursEnvironment();
};

void TestSampleLog::appendsJsonlAndContinuesPerSol()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("samples.jsonl"));

    SampleLog log(path);
    QString error;
    QVERIFY2(log.load(&error), qPrintable(error));
    QCOMPARE(log.sampleCount(), 0);

    SampleRecord first = makeRecord(3, QStringLiteral("T1"));
    QVERIFY2(log.append(&first, &error), qPrintable(error));
    QCOMPARE(first.id, QStringLiteral("SAMPLE-3-1"));

    SampleRecord second = makeRecord(3, QStringLiteral("T2"));
    second.targetName = QStringLiteral("Marias Pass");
    QVERIFY2(log.append(&second, &error), qPrintable(error));
    QCOMPARE(second.id, QStringLiteral("SAMPLE-3-2"));

    SampleRecord third = makeRecord(4, QStringLiteral("T1"));
    QVERIFY2(log.append(&third, &error), qPrintable(error));
    QCOMPARE(third.id, QStringLiteral("SAMPLE-4-1"));

    QCOMPARE(lineCount(path), 3);
    QCOMPARE(log.sampleCount(), 3);

    SampleLog reloaded(path);
    QVERIFY2(reloaded.load(&error), qPrintable(error));
    QCOMPARE(reloaded.sampleCount(), 3);
    QCOMPARE(reloaded.records().at(0).id, QStringLiteral("SAMPLE-3-1"));
    QCOMPARE(reloaded.records().at(1).id, QStringLiteral("SAMPLE-3-2"));
    QCOMPARE(reloaded.records().at(2).id, QStringLiteral("SAMPLE-4-1"));
    QCOMPARE(reloaded.nextId(3), QStringLiteral("SAMPLE-3-3"));
    QCOMPARE(reloaded.nextId(4), QStringLiteral("SAMPLE-4-2"));

    const SampleRecord &loadedFirst = reloaded.records().at(0);
    QCOMPARE(loadedFirst.sol, 3);
    QVERIFY(loadedFirst.timestampUtc.isValid());
    QCOMPARE(loadedFirst.targetId, first.targetId);
    QCOMPARE(loadedFirst.targetName, first.targetName);
    QCOMPARE(loadedFirst.targetSource, first.targetSource);
    QCOMPARE(loadedFirst.eastingM, first.eastingM);
    QCOMPARE(loadedFirst.northingM, first.northingM);
    QCOMPARE(loadedFirst.lonDeg, first.lonDeg);
    QCOMPARE(loadedFirst.latDeg, first.latDeg);
    QCOMPARE(loadedFirst.upEast, first.upEast);
    QCOMPARE(loadedFirst.upNorth, first.upNorth);
    QCOMPARE(loadedFirst.upUp, first.upUp);
    QCOMPARE(loadedFirst.slopeDeg, first.slopeDeg);
    QCOMPARE(loadedFirst.aspectDeg, first.aspectDeg);
    QCOMPARE(loadedFirst.aspectDefined, first.aspectDefined);
    QCOMPARE(loadedFirst.orientationMethod, first.orientationMethod);
    QCOMPARE(loadedFirst.orientationSource, first.orientationSource);
    QCOMPARE(loadedFirst.phenomenon, first.phenomenon);
    QCOMPARE(loadedFirst.why, first.why);
    QCOMPARE(loadedFirst.measurement, first.measurement);
    QCOMPARE(loadedFirst.positionSource, first.positionSource);
    QCOMPARE(loadedFirst.compositionMeasured, false);
}

void TestSampleLog::rejectsInvalidRecord()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    {
        const QString path = dir.filePath(QStringLiteral("missing.jsonl"));
        SampleLog log(path);
        SampleRecord record = makeRecord(3, QStringLiteral("T1"));
        record.phenomenon.clear();
        QString error;
        QVERIFY(!log.append(&record, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!QFile::exists(path));
    }

    {
        const QString path = dir.filePath(QStringLiteral("nan.jsonl"));
        SampleLog log(path);
        SampleRecord record = makeRecord(3, QStringLiteral("T1"));
        record.eastingM = std::numeric_limits<double>::quiet_NaN();
        QString error;
        QVERIFY(!log.append(&record, &error));
        QVERIFY(!QFile::exists(path));
    }

    {
        const QString path = dir.filePath(QStringLiteral("normal.jsonl"));
        SampleLog log(path);
        SampleRecord record = makeRecord(3, QStringLiteral("T1"));
        record.upEast = 1.0;
        record.upNorth = 1.0;
        record.upUp = 1.0;
        QString error;
        QVERIFY(!log.append(&record, &error));
        QVERIFY(!QFile::exists(path));
    }

    {
        const QString path = dir.filePath(QStringLiteral("malformed-id.jsonl"));
        SampleLog log(path);
        SampleRecord record = makeRecord(3, QStringLiteral("T1"));
        record.id = QStringLiteral("SAMPLE-3-x");
        QString error;
        QVERIFY(!log.append(&record, &error));
        QVERIFY2(error.contains(QStringLiteral("SAMPLE-<sol>-<n>")),
                 qPrintable(error));
        QVERIFY(!QFile::exists(path));
    }

    {
        const QString path = dir.filePath(QStringLiteral("wrong-sol.jsonl"));
        SampleLog log(path);
        SampleRecord record = makeRecord(3, QStringLiteral("T1"));
        record.id = QStringLiteral("SAMPLE-4-2");
        QString error;
        QVERIFY(!log.append(&record, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!QFile::exists(path));
    }

    {
        const QString path = dir.filePath(QStringLiteral("empty-measurement.jsonl"));
        SampleLog log(path);
        SampleRecord record = makeRecord(3, QStringLiteral("T1"));
        record.measurement.clear();
        QString error;
        QVERIFY(!log.append(&record, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!QFile::exists(path));
    }

    {
        const QString path = dir.filePath(QStringLiteral("empty-position.jsonl"));
        SampleLog log(path);
        SampleRecord record = makeRecord(3, QStringLiteral("T1"));
        record.positionSource.clear();
        QString error;
        QVERIFY(!log.append(&record, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!QFile::exists(path));
    }

    {
        const QString path = dir.filePath(QStringLiteral("composition.jsonl"));
        SampleLog log(path);
        SampleRecord record = makeRecord(3, QStringLiteral("T1"));
        record.compositionMeasured = true;
        QString error;
        QVERIFY(!log.append(&record, &error));
        QVERIFY2(error.contains(QStringLiteral("composition_measured")),
                 qPrintable(error));
        QVERIFY(!QFile::exists(path));
    }

    {
        const QString path = dir.filePath(QStringLiteral("duplicate.jsonl"));
        SampleLog log(path);
        SampleRecord record = makeRecord(3, QStringLiteral("T1"));
        QString error;
        QVERIFY2(log.append(&record, &error), qPrintable(error));
        SampleRecord duplicate = record;
        QVERIFY(!log.append(&duplicate, &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(lineCount(path), 1);
    }
}

void TestSampleLog::corruptLogFailsLoudly()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("corrupt.jsonl"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("not json\n");
    file.close();

    SampleLog log(path);
    QString error;
    QVERIFY(!log.load(&error));
    QVERIFY2(error.contains(path), qPrintable(error));
    QVERIFY2(error.contains(QStringLiteral("line 1")), qPrintable(error));
}

void TestSampleLog::defaultPathHonoursEnvironment()
{
    const bool hadOverride = qEnvironmentVariableIsSet("CETUS_SAMPLE_LOG");
    const QByteArray previous = qgetenv("CETUS_SAMPLE_LOG");
    const QByteArray overridePath("/tmp/cetus-test-samples.jsonl");
    qputenv("CETUS_SAMPLE_LOG", overridePath);
    QCOMPARE(SampleLog::defaultPath(), QString::fromUtf8(overridePath));

    if (hadOverride)
        qputenv("CETUS_SAMPLE_LOG", previous);
    else
        qunsetenv("CETUS_SAMPLE_LOG");
}

QTEST_MAIN(TestSampleLog)
#include "test_samplelog.moc"
