#pragma once

// M1a dataset registry: opens the four prepared rasters from SiteConfig and
// keeps them in display order. Fails loudly (with id + path) when any layer
// is missing or invalid — never a silent skip (AGENTS.md §6).

#include "data/RasterLayer.h"

#include <QString>
#include <QStringList>

#include <memory>
#include <vector>

class DatasetRegistry
{
public:
    DatasetRegistry() = default;

    // Loads the four prepared layers (hillshade, dem, slope, optical).
    // Returns false and writes "<id> <path>: <gdal error>" to *errorOut on
    // the first failure; the registry is left empty in that case.
    bool loadPrepared(QString *errorOut);

    const std::vector<std::unique_ptr<RasterLayer>> &layers() const;
    const RasterLayer *layer(const QString &id) const;
    // Layer shown by default: hirise_optical.
    const RasterLayer *defaultLayer() const;
    QStringList ids() const;

private:
    std::vector<std::unique_ptr<RasterLayer>> m_layers;
};
