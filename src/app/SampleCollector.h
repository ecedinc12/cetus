#pragma once

// M6 sample collector: turns a science target pin into one honest field-note
// record. Terrain orientation is context only; no composition measurement is
// claimed. Any missing DEM cell, missing phenomenon or invalid timestamp is a
// hard failure and writes nothing.

#include "analysis/TargetRanking.h"
#include "app/SampleLog.h"

#include <QDateTime>
#include <QString>

class RasterLayer;

class SampleCollector
{
public:
    SampleCollector(const RasterLayer *dem, QString logPath);

    bool load(QString *errorOut = nullptr);
    bool isReady() const;

    bool collect(const ScienceTarget &target, int sol,
                 const QDateTime &timestampUtc, SampleRecord *out,
                 QString *errorOut = nullptr);

    const SampleLog &log() const;
    QString logPath() const;

private:
    const RasterLayer *m_dem = nullptr;
    SampleLog m_log;
    bool m_loaded = false;
};
