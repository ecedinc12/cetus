#pragma once

// M6 session sample log: one JSONL field-note record per collected sample.
// This is a field note, never a claim that a physical sample or composition
// measurement was acquired. Invalid records and malformed logs fail loudly.

#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QVector>

struct SampleRecord
{
    QString id;  // SAMPLE-<sol>-<n>
    int sol = -1;
    QDateTime timestampUtc;

    QString targetId, targetName, targetSource;
    double eastingM = 0.0, northingM = 0.0;
    double lonDeg = 0.0, latDeg = 0.0;

    double upEast = 0.0, upNorth = 0.0, upUp = 1.0;
    double slopeDeg = 0.0, aspectDeg = 0.0;
    bool aspectDefined = false;
    QString orientationMethod, orientationSource;

    QString phenomenon, why, measurement;
    QString positionSource;
    bool compositionMeasured = false;
};

class SampleLog
{
public:
    explicit SampleLog(QString filePath);

    // A missing file is an empty log. Otherwise every non-blank line is
    // parsed strictly; a malformed line, duplicate id or bad schema fails
    // with the file path and line number.
    bool load(QString *errorOut = nullptr);

    // Validates the record, assigns record->id when empty, creates parent
    // directories and appends exactly one compact JSON object + newline.
    bool append(SampleRecord *record, QString *errorOut = nullptr);

    QString nextId(int sol) const;
    const QVector<SampleRecord> &records() const;
    QString filePath() const;
    int sampleCount() const;

    // Non-empty CETUS_SAMPLE_LOG wins; else <AppData>/samples.jsonl. Empty
    // when no writable application-data location is known.
    static QString defaultPath();
    static QJsonObject toJson(const SampleRecord &record);
    static bool fromJson(const QJsonObject &object, SampleRecord *record,
                         QString *errorOut = nullptr);

    // Shared schema/finite/range validation (also used by append).
    static bool validate(const SampleRecord &record, QString *errorOut = nullptr);

private:
    QString m_filePath;
    QVector<SampleRecord> m_records;
};
