#pragma once

// M3 scenario: NASA-derived crew start and science targets loaded from the
// shipped JSON. The loader validates the Mars CRS and every required field;
// `out` is only modified on success (AGENTS.md §6: fail loudly).

#include "analysis/TargetRanking.h"

#include <QPointF>
#include <QString>
#include <QVector>

struct MissionScenario
{
    QString source;
    QString crewName;
    QString crewSource;
    QPointF crewStart;
    QVector<ScienceTarget> targets;

    static bool loadFromFile(const QString &path, MissionScenario *out,
                             QString *errorOut = nullptr);
};
