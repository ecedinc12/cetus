#include "app/SampleLog.h"

#include "data/SiteConfig.h"
#include "gis/PlanetaryCrs.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>

#include <cmath>
#include <utility>

namespace
{

constexpr const char *kSchema = "cetus.sample/1";

bool fail(QString *errorOut, const QString &message)
{
    if (errorOut)
        *errorOut = message;
    return false;
}

bool isFinite(const QJsonValue &value)
{
    return value.isDouble() && std::isfinite(value.toDouble());
}

bool requireString(const QJsonObject &object, const QString &key,
                   const QString &context, QString *out, QString *error,
                   bool nonEmpty = true)
{
    const QJsonValue value = object.value(key);
    if (!value.isString() || (nonEmpty && value.toString().isEmpty()))
        return fail(error, QStringLiteral("%1: missing or empty '%2'")
                              .arg(context, key));
    *out = value.toString();
    return true;
}

bool requireNumber(const QJsonObject &object, const QString &key,
                   const QString &context, double *out, QString *error)
{
    const QJsonValue value = object.value(key);
    if (!isFinite(value))
        return fail(error, QStringLiteral("%1: missing or non-finite '%2'")
                              .arg(context, key));
    *out = value.toDouble();
    return true;
}

bool requireBool(const QJsonObject &object, const QString &key,
                 const QString &context, bool *out, QString *error)
{
    const QJsonValue value = object.value(key);
    if (!value.isBool())
        return fail(error, QStringLiteral("%1: missing or non-boolean '%2'")
                              .arg(context, key));
    *out = value.toBool();
    return true;
}

bool requireObject(const QJsonObject &object, const QString &key,
                   const QString &context, QJsonObject *out, QString *error)
{
    const QJsonValue value = object.value(key);
    if (!value.isObject())
        return fail(error, QStringLiteral("%1: missing or non-object '%2'")
                              .arg(context, key));
    *out = value.toObject();
    return true;
}

QJsonObject crsObject()
{
    QJsonObject crs;
    crs.insert(QStringLiteral("name"),
               QStringLiteral("Mars equirectangular, planetocentric"));
    crs.insert(QStringLiteral("proj4"),
               PlanetaryCrs::marsEquirectangular(
                   SiteConfig::kCentralMeridianDeg));
    crs.insert(QStringLiteral("radius_m"), SiteConfig::kMarsRadiusM);
    crs.insert(QStringLiteral("central_meridian_deg"),
               SiteConfig::kCentralMeridianDeg);
    return crs;
}

}  // namespace

SampleLog::SampleLog(QString filePath)
    : m_filePath(std::move(filePath))
{
}

QString SampleLog::filePath() const
{
    return m_filePath;
}

const QVector<SampleRecord> &SampleLog::records() const
{
    return m_records;
}

int SampleLog::sampleCount() const
{
    return static_cast<int>(m_records.size());
}

QString SampleLog::defaultPath()
{
    const QByteArray overridePath = qgetenv("CETUS_SAMPLE_LOG");
    if (!overridePath.isEmpty())
        return QString::fromUtf8(overridePath);

    const QString dir = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation);
    if (dir.isEmpty())
        return QString();
    return dir + QStringLiteral("/samples.jsonl");
}

QString SampleLog::nextId(int sol) const
{
    const QString prefix = QStringLiteral("SAMPLE-%1-").arg(sol);
    int maximum = 0;
    for (const SampleRecord &record : m_records)
    {
        if (!record.id.startsWith(prefix))
            continue;
        bool ok = false;
        const int number = record.id.mid(prefix.size()).toInt(&ok);
        if (ok && number > maximum)
            maximum = number;
    }
    return prefix + QString::number(maximum + 1);
}

bool SampleLog::validate(const SampleRecord &record, QString *errorOut)
{
    if (record.id.trimmed().isEmpty())
        return fail(errorOut, QStringLiteral("sample id is empty"));
    if (record.sol < 0)
        return fail(errorOut,
                    QStringLiteral("sample %1: sol %2 is invalid")
                        .arg(record.id)
                        .arg(record.sol));

    // The id is identity, not a label: SAMPLE-<sol>-<n> with the sol matching
    // the record and a positive per-sol sequence number.
    static const QRegularExpression idPattern(
        QStringLiteral("^SAMPLE-([0-9]+)-([0-9]+)$"));
    const QRegularExpressionMatch idMatch = idPattern.match(record.id);
    if (!idMatch.hasMatch())
        return fail(errorOut,
                    QStringLiteral("sample id '%1' is malformed; expected "
                                   "SAMPLE-<sol>-<n>")
                        .arg(record.id));
    const int idSol = idMatch.captured(1).toInt();
    const int idNumber = idMatch.captured(2).toInt();
    if (idSol != record.sol || idNumber < 1)
        return fail(errorOut,
                    QStringLiteral("sample id '%1' is malformed; expected "
                                   "SAMPLE-<sol>-<n> with sol %2 and n >= 1")
                        .arg(record.id)
                        .arg(record.sol));

    if (!record.timestampUtc.isValid())
        return fail(errorOut,
                    QStringLiteral("sample %1: timestamp is invalid")
                        .arg(record.id));

    if (record.targetId.trimmed().isEmpty())
        return fail(errorOut,
                    QStringLiteral("sample %1: target id is empty")
                        .arg(record.id));
    if (record.targetName.trimmed().isEmpty())
        return fail(errorOut,
                    QStringLiteral("sample %1: target name is empty")
                        .arg(record.id));
    if (record.targetSource.trimmed().isEmpty())
        return fail(errorOut,
                    QStringLiteral("sample %1: target source is empty")
                        .arg(record.id));

    if (!std::isfinite(record.eastingM) ||
        !std::isfinite(record.northingM))
        return fail(errorOut,
                    QStringLiteral("sample %1: position is not finite")
                        .arg(record.id));
    if (!std::isfinite(record.lonDeg) || !std::isfinite(record.latDeg))
        return fail(errorOut,
                    QStringLiteral("sample %1: lon/lat is not finite")
                        .arg(record.id));
    if (record.lonDeg < -180.0 || record.lonDeg > 180.0)
        return fail(errorOut,
                    QStringLiteral("sample %1: lon %2 is outside -180..180")
                        .arg(record.id)
                        .arg(record.lonDeg, 0, 'g', 12));
    if (record.latDeg < -90.0 || record.latDeg > 90.0)
        return fail(errorOut,
                    QStringLiteral("sample %1: lat %2 is outside -90..90")
                        .arg(record.id)
                        .arg(record.latDeg, 0, 'g', 12));

    if (!std::isfinite(record.upEast) || !std::isfinite(record.upNorth) ||
        !std::isfinite(record.upUp))
        return fail(errorOut,
                    QStringLiteral("sample %1: orientation normal is not finite")
                        .arg(record.id));
    const double normalLength =
        std::hypot(std::hypot(record.upEast, record.upNorth), record.upUp);
    if (std::fabs(normalLength - 1.0) > 1e-3)
        return fail(errorOut,
                    QStringLiteral("sample %1: orientation normal length %2 "
                                   "is not unit length")
                        .arg(record.id)
                        .arg(normalLength, 0, 'g', 12));
    if (record.upUp <= 0.0)
        return fail(errorOut,
                    QStringLiteral("sample %1: up_up %2 is not above the "
                                   "horizon")
                        .arg(record.id)
                        .arg(record.upUp, 0, 'g', 12));

    if (!std::isfinite(record.slopeDeg) || record.slopeDeg < 0.0 ||
        record.slopeDeg > 90.0)
        return fail(errorOut,
                    QStringLiteral("sample %1: slope %2 is outside 0..90")
                        .arg(record.id)
                        .arg(record.slopeDeg, 0, 'g', 12));
    if (!std::isfinite(record.aspectDeg))
        return fail(errorOut,
                    QStringLiteral("sample %1: aspect is not finite")
                        .arg(record.id));
    if (record.aspectDefined &&
        (record.aspectDeg < 0.0 || record.aspectDeg >= 360.0))
        return fail(errorOut,
                    QStringLiteral("sample %1: aspect %2 is outside 0..360")
                        .arg(record.id)
                        .arg(record.aspectDeg, 0, 'g', 12));

    if (record.phenomenon.trimmed().isEmpty())
        return fail(errorOut,
                    QStringLiteral("sample %1: phenomenon is empty")
                        .arg(record.id));
    if (record.why.trimmed().isEmpty())
        return fail(errorOut,
                    QStringLiteral("sample %1: why is empty").arg(record.id));

    if (record.orientationMethod.trimmed().isEmpty())
        return fail(errorOut,
                    QStringLiteral("sample %1: orientation method is empty")
                        .arg(record.id));
    if (record.orientationSource.trimmed().isEmpty())
        return fail(errorOut,
                    QStringLiteral("sample %1: orientation source is empty")
                        .arg(record.id));
    if (record.positionSource.trimmed().isEmpty())
        return fail(errorOut,
                    QStringLiteral("sample %1: position source is empty")
                        .arg(record.id));
    if (record.measurement.trimmed().isEmpty())
        return fail(errorOut,
                    QStringLiteral("sample %1: measurement is empty")
                        .arg(record.id));

    // M6 honesty rule: a field note must never claim a composition result.
    if (record.compositionMeasured)
        return fail(errorOut,
                    QStringLiteral("composition_measured must be false for a "
                                   "terrain-only field note"));

    if (errorOut)
        errorOut->clear();
    return true;
}

QJsonObject SampleLog::toJson(const SampleRecord &record)
{
    QJsonObject target;
    target.insert(QStringLiteral("id"), record.targetId);
    target.insert(QStringLiteral("name"), record.targetName);
    target.insert(QStringLiteral("source"), record.targetSource);

    QJsonObject position;
    position.insert(QStringLiteral("easting_m"), record.eastingM);
    position.insert(QStringLiteral("northing_m"), record.northingM);
    position.insert(QStringLiteral("lon_deg"), record.lonDeg);
    position.insert(QStringLiteral("lat_deg"), record.latDeg);
    position.insert(QStringLiteral("crs"), crsObject());

    QJsonObject orientation;
    orientation.insert(QStringLiteral("up_east"), record.upEast);
    orientation.insert(QStringLiteral("up_north"), record.upNorth);
    orientation.insert(QStringLiteral("up_up"), record.upUp);
    orientation.insert(QStringLiteral("slope_deg"), record.slopeDeg);
    orientation.insert(QStringLiteral("aspect_deg"), record.aspectDeg);
    orientation.insert(QStringLiteral("aspect_defined"),
                       record.aspectDefined);
    orientation.insert(QStringLiteral("method"), record.orientationMethod);
    orientation.insert(QStringLiteral("source"), record.orientationSource);

    QJsonObject fieldNote;
    fieldNote.insert(QStringLiteral("phenomenon"), record.phenomenon);
    fieldNote.insert(QStringLiteral("why"), record.why);
    fieldNote.insert(QStringLiteral("measurement"), record.measurement);
    fieldNote.insert(QStringLiteral("composition_measured"),
                     record.compositionMeasured);

    QJsonObject provenance;
    provenance.insert(
        QStringLiteral("recorded_by"),
        QStringLiteral("Cetus field tool (operator-entered field note)"));
    provenance.insert(QStringLiteral("position_source"), record.positionSource);
    provenance.insert(QStringLiteral("orientation_source"),
                      record.orientationSource);
    provenance.insert(
        QStringLiteral("note"),
        QStringLiteral("Field note only; no physical sample or composition "
                       "measurement is claimed."));

    QJsonObject root;
    root.insert(QStringLiteral("schema"), QString::fromLatin1(kSchema));
    root.insert(QStringLiteral("id"), record.id);
    root.insert(QStringLiteral("sol"), record.sol);
    root.insert(QStringLiteral("timestamp_utc"),
                record.timestampUtc.toUTC().toString(Qt::ISODateWithMs));
    root.insert(QStringLiteral("target"), target);
    root.insert(QStringLiteral("position"), position);
    root.insert(QStringLiteral("orientation"), orientation);
    root.insert(QStringLiteral("field_note"), fieldNote);
    root.insert(QStringLiteral("provenance"), provenance);
    return root;
}

bool SampleLog::fromJson(const QJsonObject &object, SampleRecord *record,
                         QString *errorOut)
{
    if (!record)
        return fail(errorOut, QStringLiteral("no sample record output"));

    QString error;
    QString schema;
    if (!requireString(object, QStringLiteral("schema"),
                       QStringLiteral("sample"), &schema, errorOut))
        return false;
    if (schema != QString::fromLatin1(kSchema))
        return fail(errorOut,
                    QStringLiteral("unsupported sample schema '%1' (expected "
                                   "'%2')")
                        .arg(schema, QString::fromLatin1(kSchema)));

    SampleRecord parsed;

    QString id;
    if (!requireString(object, QStringLiteral("id"),
                       QStringLiteral("sample"), &id, errorOut))
        return false;
    parsed.id = id;

    const QJsonValue solValue = object.value(QStringLiteral("sol"));
    if (!solValue.isDouble() || !std::isfinite(solValue.toDouble()) ||
        std::floor(solValue.toDouble()) != solValue.toDouble())
        return fail(errorOut,
                    QStringLiteral("sample %1: missing or invalid 'sol'")
                        .arg(parsed.id));
    parsed.sol = solValue.toInt();

    QString timestampText;
    if (!requireString(object, QStringLiteral("timestamp_utc"),
                       QStringLiteral("sample"), &timestampText, errorOut))
        return false;
    parsed.timestampUtc =
        QDateTime::fromString(timestampText, Qt::ISODateWithMs);
    if (!parsed.timestampUtc.isValid())
        parsed.timestampUtc = QDateTime::fromString(timestampText, Qt::ISODate);
    if (!parsed.timestampUtc.isValid())
        return fail(errorOut,
                    QStringLiteral("sample %1: invalid timestamp_utc '%2'")
                        .arg(parsed.id, timestampText));

    QJsonObject target;
    if (!requireObject(object, QStringLiteral("target"),
                       QStringLiteral("sample %1").arg(parsed.id), &target,
                       errorOut))
        return false;
    const QString targetContext = QStringLiteral("sample %1 target").arg(parsed.id);
    if (!requireString(target, QStringLiteral("id"), targetContext,
                       &parsed.targetId, errorOut) ||
        !requireString(target, QStringLiteral("name"), targetContext,
                       &parsed.targetName, errorOut) ||
        !requireString(target, QStringLiteral("source"), targetContext,
                       &parsed.targetSource, errorOut))
        return false;

    QJsonObject position;
    if (!requireObject(object, QStringLiteral("position"),
                       QStringLiteral("sample %1").arg(parsed.id), &position,
                       errorOut))
        return false;
    const QString positionContext =
        QStringLiteral("sample %1 position").arg(parsed.id);
    if (!requireNumber(position, QStringLiteral("easting_m"), positionContext,
                       &parsed.eastingM, errorOut) ||
        !requireNumber(position, QStringLiteral("northing_m"), positionContext,
                       &parsed.northingM, errorOut) ||
        !requireNumber(position, QStringLiteral("lon_deg"), positionContext,
                       &parsed.lonDeg, errorOut) ||
        !requireNumber(position, QStringLiteral("lat_deg"), positionContext,
                       &parsed.latDeg, errorOut))
        return false;
    QJsonObject crs;
    if (!requireObject(position, QStringLiteral("crs"), positionContext, &crs,
                       errorOut))
        return false;
    QString crsField;
    if (!requireString(crs, QStringLiteral("name"),
                       positionContext + QStringLiteral(" crs"), &crsField,
                       errorOut) ||
        !requireString(crs, QStringLiteral("proj4"),
                       positionContext + QStringLiteral(" crs"), &crsField,
                       errorOut))
        return false;
    double crsNumber = 0.0;
    if (!requireNumber(crs, QStringLiteral("radius_m"),
                       positionContext + QStringLiteral(" crs"), &crsNumber,
                       errorOut) ||
        !requireNumber(crs, QStringLiteral("central_meridian_deg"),
                       positionContext + QStringLiteral(" crs"), &crsNumber,
                       errorOut))
        return false;

    QJsonObject orientation;
    if (!requireObject(object, QStringLiteral("orientation"),
                       QStringLiteral("sample %1").arg(parsed.id),
                       &orientation, errorOut))
        return false;
    const QString orientationContext =
        QStringLiteral("sample %1 orientation").arg(parsed.id);
    if (!requireNumber(orientation, QStringLiteral("up_east"),
                       orientationContext, &parsed.upEast, errorOut) ||
        !requireNumber(orientation, QStringLiteral("up_north"),
                       orientationContext, &parsed.upNorth, errorOut) ||
        !requireNumber(orientation, QStringLiteral("up_up"),
                       orientationContext, &parsed.upUp, errorOut) ||
        !requireNumber(orientation, QStringLiteral("slope_deg"),
                       orientationContext, &parsed.slopeDeg, errorOut) ||
        !requireNumber(orientation, QStringLiteral("aspect_deg"),
                       orientationContext, &parsed.aspectDeg, errorOut) ||
        !requireBool(orientation, QStringLiteral("aspect_defined"),
                     orientationContext, &parsed.aspectDefined, errorOut) ||
        !requireString(orientation, QStringLiteral("method"),
                       orientationContext, &parsed.orientationMethod,
                       errorOut, false) ||
        !requireString(orientation, QStringLiteral("source"),
                       orientationContext, &parsed.orientationSource,
                       errorOut, false))
        return false;

    QJsonObject fieldNote;
    if (!requireObject(object, QStringLiteral("field_note"),
                       QStringLiteral("sample %1").arg(parsed.id), &fieldNote,
                       errorOut))
        return false;
    const QString noteContext =
        QStringLiteral("sample %1 field_note").arg(parsed.id);
    if (!requireString(fieldNote, QStringLiteral("phenomenon"), noteContext,
                       &parsed.phenomenon, errorOut) ||
        !requireString(fieldNote, QStringLiteral("why"), noteContext,
                       &parsed.why, errorOut) ||
        !requireString(fieldNote, QStringLiteral("measurement"), noteContext,
                       &parsed.measurement, errorOut, false) ||
        !requireBool(fieldNote, QStringLiteral("composition_measured"),
                     noteContext, &parsed.compositionMeasured, errorOut))
        return false;

    QJsonObject provenance;
    if (!requireObject(object, QStringLiteral("provenance"),
                       QStringLiteral("sample %1").arg(parsed.id), &provenance,
                       errorOut))
        return false;
    const QString provenanceContext =
        QStringLiteral("sample %1 provenance").arg(parsed.id);
    QString recordedBy;
    QString note;
    if (!requireString(provenance, QStringLiteral("recorded_by"),
                       provenanceContext, &recordedBy, errorOut, false) ||
        !requireString(provenance, QStringLiteral("position_source"),
                       provenanceContext, &parsed.positionSource, errorOut,
                       false) ||
        !requireString(provenance, QStringLiteral("orientation_source"),
                       provenanceContext, &parsed.orientationSource,
                       errorOut, false) ||
        !requireString(provenance, QStringLiteral("note"), provenanceContext,
                       &note, errorOut, false))
        return false;

    if (!validate(parsed, errorOut))
        return false;

    *record = parsed;
    return true;
}

bool SampleLog::load(QString *errorOut)
{
    if (m_filePath.isEmpty())
        return fail(errorOut,
                    QStringLiteral("sample log path is empty; cannot load "
                                   "the session log"));

    m_records.clear();

    if (!QFile::exists(m_filePath))
    {
        if (errorOut)
            errorOut->clear();
        return true;
    }

    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly))
        return fail(errorOut,
                    QStringLiteral("cannot open sample log '%1': %2")
                        .arg(m_filePath, file.errorString()));

    QVector<SampleRecord> loaded;
    QSet<QString> seenIds;
    int lineNumber = 0;
    while (!file.atEnd())
    {
        const QByteArray line = file.readLine();
        ++lineNumber;
        const QString text = QString::fromUtf8(line).trimmed();
        if (text.isEmpty())
            continue;

        QJsonParseError parseError;
        const QJsonDocument document =
            QJsonDocument::fromJson(text.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError)
            return fail(errorOut,
                        QStringLiteral("sample log '%1' line %2: invalid JSON "
                                       "at offset %3: %4")
                            .arg(m_filePath)
                            .arg(lineNumber)
                            .arg(parseError.offset)
                            .arg(parseError.errorString()));
        if (!document.isObject())
            return fail(errorOut,
                        QStringLiteral("sample log '%1' line %2: record is not "
                                       "a JSON object")
                            .arg(m_filePath)
                            .arg(lineNumber));

        SampleRecord record;
        QString recordError;
        if (!fromJson(document.object(), &record, &recordError))
            return fail(errorOut,
                        QStringLiteral("sample log '%1' line %2: %3")
                            .arg(m_filePath)
                            .arg(lineNumber)
                            .arg(recordError));
        if (seenIds.contains(record.id))
            return fail(errorOut,
                        QStringLiteral("sample log '%1' line %2: duplicate "
                                       "sample id '%3'")
                            .arg(m_filePath)
                            .arg(lineNumber)
                            .arg(record.id));
        seenIds.insert(record.id);
        loaded.push_back(record);
    }

    if (file.error() != QFileDevice::NoError)
        return fail(errorOut,
                    QStringLiteral("cannot read sample log '%1': %2")
                        .arg(m_filePath, file.errorString()));

    m_records = loaded;
    if (errorOut)
        errorOut->clear();
    return true;
}

bool SampleLog::append(SampleRecord *record, QString *errorOut)
{
    if (!record)
        return fail(errorOut, QStringLiteral("no sample record provided"));

    if (m_filePath.isEmpty())
        return fail(errorOut,
                    QStringLiteral("sample log path is empty; refusing to "
                                   "write a field note"));

    SampleRecord pending = *record;
    if (pending.id.trimmed().isEmpty())
    {
        if (pending.sol < 0)
            return fail(errorOut,
                        QStringLiteral("cannot assign a sample id: sol %1 is "
                                       "invalid")
                            .arg(pending.sol));
        pending.id = nextId(pending.sol);
    }

    if (!validate(pending, errorOut))
        return false;

    for (const SampleRecord &existing : m_records) {
        if (existing.id == pending.id)
            return fail(errorOut,
                        QStringLiteral("duplicate sample id '%1'; refusing to "
                                       "append a second field note")
                            .arg(pending.id));
    }

    const QJsonObject object = toJson(pending);
    QByteArray line = QJsonDocument(object).toJson(QJsonDocument::Compact);
    line.append('\n');

    const QString directory = QFileInfo(m_filePath).absolutePath();
    if (directory.isEmpty() ||
        !QDir().mkpath(directory))
        return fail(errorOut,
                    QStringLiteral("cannot create sample log directory '%1'")
                        .arg(directory));

    QFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append))
        return fail(errorOut,
                    QStringLiteral("cannot open sample log '%1' for append: %2")
                        .arg(m_filePath, file.errorString()));

    const qint64 written = file.write(line);
    if (written != line.size())
        return fail(errorOut,
                    QStringLiteral("cannot write sample log '%1': %2")
                        .arg(m_filePath, file.errorString()));
    if (!file.flush())
        return fail(errorOut,
                    QStringLiteral("cannot flush sample log '%1': %2")
                        .arg(m_filePath, file.errorString()));
    if (file.error() != QFileDevice::NoError)
        return fail(errorOut,
                    QStringLiteral("cannot write sample log '%1': %2")
                        .arg(m_filePath, file.errorString()));

    m_records.push_back(pending);
    *record = pending;
    if (errorOut)
        errorOut->clear();
    return true;
}
