#pragma once

// Science targets and value-rate ranking (PLAN.md §5.2):
//   value_rate = science_value / (travel_minutes + minutes_on_target)
// Reachability/walkback is decided by the planner; this module only ranks.

#include <QHash>
#include <QPointF>
#include <QString>
#include <QVector>

struct ScienceTarget
{
    QString id;             // "T1" etc.
    QString name;
    QString why;
    QString source;
    double eastingM = 0.0;
    double northingM = 0.0;
    double scienceValue = 0.0;    // 0..1, scenario design choice
    double minutesOnTarget = 0.0;
    QString phenomenon;           // observed feature, e.g. "boxwork vein"
    QPointF position() const { return QPointF(eastingM, northingM); }
};

struct RankedTarget
{
    ScienceTarget target;
    double travelMinutes = 0.0;
    double valueRate = 0.0;
    bool reachable = false;
};

class TargetRanking
{
public:
    static double valueRate(double scienceValue, double travelMinutes,
                            double minutesOnTarget);
    static QVector<RankedTarget> rank(
        const QVector<ScienceTarget> &targets,
        const QHash<QString, double> &travelMinutesById);
};
