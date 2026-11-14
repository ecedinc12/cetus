#include "app/MissionScenario.h"

#include "data/SiteConfig.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <cmath>

namespace
{

bool readRequiredString(const QJsonObject &object, const QString &key,
                        const QString &context, QString *value,
                        QString *error)
{
    const QJsonValue field = object.value(key);
    if (!field.isString())
    {
        *error = QStringLiteral("%1: missing or non-string '%2'")
                     .arg(context, key);
        return false;
    }
    *value = field.toString();
    return true;
}

bool readRequiredNumber(const QJsonObject &object, const QString &key,
                        const QString &context, double *value,
                        QString *error)
{
    const QJsonValue field = object.value(key);
    if (!field.isDouble() || !std::isfinite(field.toDouble()))
    {
        *error = QStringLiteral("%1: missing or non-finite '%2'")
                     .arg(context, key);
        return false;
    }
    *value = field.toDouble();
    return true;
}

}  // namespace

bool MissionScenario::loadFromFile(const QString &path, MissionScenario *out,
                                   QString *errorOut)
{
    auto fail = [path, errorOut](const QString &message) {
        if (errorOut)
            *errorOut = QStringLiteral("%1: %2").arg(path, message);
        return false;
    };

    if (errorOut)
        errorOut->clear();

    if (!out)
        return fail(QStringLiteral("no output scenario provided"));

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return fail(QStringLiteral("cannot open scenario file: %1")
                        .arg(file.errorString()));

    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError)
        return fail(QStringLiteral("invalid JSON at offset %1: %2")
                        .arg(parseError.offset)
                        .arg(parseError.errorString()));
    if (!document.isObject())
        return fail(QStringLiteral("scenario root is not a JSON object"));

    const QJsonObject root = document.object();
    QString schema;
    QString error;
    if (!readRequiredString(root, QStringLiteral("schema"),
                            QStringLiteral("scenario"), &schema, &error))
        return fail(error);
    if (schema != QStringLiteral("cetus.m3.scenario/1"))
        return fail(QStringLiteral("unsupported schema '%1' (expected "
                                   "'cetus.m3.scenario/1')")
                        .arg(schema));

    const QJsonValue crsValue = root.value(QStringLiteral("crs"));
    if (!crsValue.isObject())
        return fail(QStringLiteral(
            "missing 'crs' object: the Mars CRS is required"));
    const QJsonObject crs = crsValue.toObject();

    double radiusM = 0.0;
    double centralMeridianDeg = 0.0;
    if (!readRequiredNumber(crs, QStringLiteral("radius_m"),
                            QStringLiteral("crs"), &radiusM, &error) ||
        !readRequiredNumber(crs, QStringLiteral("central_meridian_deg"),
                            QStringLiteral("crs"), &centralMeridianDeg,
                            &error))
        return fail(QStringLiteral("%1: the Mars CRS is required").arg(error));

    if (std::fabs(radiusM - SiteConfig::kMarsRadiusM) > 1.0)
        return fail(QStringLiteral(
            "CRS radius_m=%1 does not match the site Mars CRS radius %2 m")
                        .arg(radiusM, 0, 'g', 12)
                        .arg(SiteConfig::kMarsRadiusM, 0, 'f', 0));
    if (std::fabs(centralMeridianDeg - SiteConfig::kCentralMeridianDeg) >
        1e-6)
        return fail(QStringLiteral(
            "CRS central_meridian_deg=%1 does not match the site Mars CRS "
            "central meridian %2 deg")
                        .arg(centralMeridianDeg, 0, 'g', 15)
                        .arg(SiteConfig::kCentralMeridianDeg, 0, 'g', 15));

    MissionScenario scenario;

    const QJsonValue sourceValue = root.value(QStringLiteral("source"));
    if (sourceValue.isString())
        scenario.source = sourceValue.toString();

    const QJsonValue crewValue = root.value(QStringLiteral("crew"));
    if (!crewValue.isObject())
        return fail(QStringLiteral("missing 'crew' object"));
    const QJsonObject crew = crewValue.toObject();
    if (!readRequiredString(crew, QStringLiteral("name"),
                            QStringLiteral("crew"), &scenario.crewName,
                            &error) ||
        !readRequiredString(crew, QStringLiteral("source"),
                            QStringLiteral("crew"), &scenario.crewSource,
                            &error))
        return fail(error);

    double crewEasting = 0.0;
    double crewNorthing = 0.0;
    if (!readRequiredNumber(crew, QStringLiteral("easting_m"),
                            QStringLiteral("crew"), &crewEasting, &error) ||
        !readRequiredNumber(crew, QStringLiteral("northing_m"),
                            QStringLiteral("crew"), &crewNorthing, &error))
        return fail(error);
    scenario.crewStart = QPointF(crewEasting, crewNorthing);

    const QJsonValue targetsValue = root.value(QStringLiteral("targets"));
    if (!targetsValue.isArray())
        return fail(QStringLiteral("missing 'targets' array"));
    const QJsonArray targets = targetsValue.toArray();

    QVector<QString> seenIds;
    seenIds.reserve(targets.size());

    for (int i = 0; i < targets.size(); ++i)
    {
        if (!targets.at(i).isObject())
            return fail(QStringLiteral("targets[%1] is not an object").arg(i));
        const QJsonObject targetObject = targets.at(i).toObject();
        const QString context = QStringLiteral("targets[%1]").arg(i);

        ScienceTarget target;
        if (!readRequiredString(targetObject, QStringLiteral("id"), context,
                                &target.id, &error) ||
            !readRequiredString(targetObject, QStringLiteral("name"),
                                context, &target.name, &error) ||
            !readRequiredString(targetObject, QStringLiteral("why"), context,
                                &target.why, &error) ||
            !readRequiredString(targetObject, QStringLiteral("source"),
                                context, &target.source, &error))
            return fail(error);

        if (seenIds.contains(target.id))
            return fail(QStringLiteral("duplicate target id '%1'")
                            .arg(target.id));
        seenIds.push_back(target.id);

        if (!readRequiredNumber(targetObject, QStringLiteral("easting_m"),
                                context, &target.eastingM, &error) ||
            !readRequiredNumber(targetObject, QStringLiteral("northing_m"),
                                context, &target.northingM, &error) ||
            !readRequiredNumber(targetObject,
                                QStringLiteral("science_value"), context,
                                &target.scienceValue, &error) ||
            !readRequiredNumber(targetObject,
                                QStringLiteral("minutes_on_target"), context,
                                &target.minutesOnTarget, &error))
            return fail(error);

        if (target.scienceValue < 0.0 || target.scienceValue > 1.0)
            return fail(QStringLiteral(
                "%1: science_value %2 is outside 0..1")
                            .arg(context)
                            .arg(target.scienceValue, 0, 'g', 12));
        if (target.minutesOnTarget <= 0.0)
            return fail(QStringLiteral(
                "%1: minutes_on_target %2 must be positive")
                            .arg(context)
                            .arg(target.minutesOnTarget, 0, 'g', 12));

        // Optional here: the sample collector refuses to write a field note
        // without it, but the scenario may still load without one.
        const QJsonValue phenomenonValue =
            targetObject.value(QStringLiteral("phenomenon"));
        if (phenomenonValue.isString())
            target.phenomenon = phenomenonValue.toString();

        scenario.targets.push_back(target);
    }

    *out = scenario;
    return true;
}
