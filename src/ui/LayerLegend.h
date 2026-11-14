#pragma once

// M2b layer legend: one glove-friendly checkbox row per prepared raster,
// each row naming the layer plus its source, resolution and units so the
// crew always knows what a visible overlay represents. The widget only reads
// DatasetRegistry metadata; it never opens GDAL (AGENTS.md §6).

#include "data/RasterLayer.h"

#include <QStringList>
#include <QWidget>

#include <vector>

class DatasetRegistry;
class QCheckBox;
class QLabel;
class QVBoxLayout;

class LayerLegend : public QWidget
{
    Q_OBJECT
public:
    explicit LayerLegend(QWidget *parent = nullptr);

    // Non-owning registry. Rebuilds the rows. `visibleIds` sets initial checks.
    void setRegistry(const DatasetRegistry *registry,
                     const QStringList &visibleIds = {});
    int layerCount() const;
    bool isLayerVisible(const QString &layerId) const;
    QStringList visibleLayerIds() const;
    QCheckBox *checkbox(const QString &layerId) const;
    QLabel *detailLabel(const QString &layerId) const;

public slots:
    void setLayerVisible(const QString &layerId, bool visible);

signals:
    void layerToggled(const QString &layerId, bool visible);

private:
    struct Row
    {
        QString id;
        QCheckBox *toggle = nullptr;
        QLabel *detail = nullptr;
    };

    void clearRows();
    const Row *findRow(const QString &layerId) const;

    const DatasetRegistry *m_registry = nullptr;
    QVBoxLayout *m_rowsLayout = nullptr;
    std::vector<Row> m_rows;
};
