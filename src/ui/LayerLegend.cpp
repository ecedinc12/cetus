#include "ui/LayerLegend.h"

#include "data/DatasetRegistry.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QVBoxLayout>

LayerLegend::LayerLegend(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(8);

    auto *title = new QLabel(tr("LAYERS"), this);
    title->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: bold;"));
    layout->addWidget(title);

    auto *hint = new QLabel(
        tr("check to show \u00b7 source \u00b7 resolution \u00b7 units"), this);
    hint->setStyleSheet(QStringLiteral("font-size: 11px; color: #9a8f86;"));
    layout->addWidget(hint);

    m_rowsLayout = new QVBoxLayout();
    m_rowsLayout->setContentsMargins(0, 4, 0, 0);
    m_rowsLayout->setSpacing(6);
    layout->addLayout(m_rowsLayout);
    layout->addStretch(1);
}

void LayerLegend::setRegistry(const DatasetRegistry *registry,
                              const QStringList &visibleIds)
{
    m_registry = registry;
    clearRows();

    if (!m_registry)
        return;

    for (const auto &raster : m_registry->layers())
    {
        const RasterLayer::Metadata &meta = raster->metadata();

        auto *rowWidget = new QWidget(this);
        auto *rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(8);

        auto *toggle = new QCheckBox(meta.title, rowWidget);
        toggle->setObjectName(QStringLiteral("layerToggle_") + meta.id);
        toggle->setStyleSheet(QStringLiteral("font-size: 14px;"));
        toggle->setMinimumWidth(190);
        toggle->setToolTip(meta.product);
        rowLayout->addWidget(toggle);

        auto *detail = new QLabel(
            meta.source + QStringLiteral(" \u00b7 ") + meta.resolutionText
                + QStringLiteral(" \u00b7 ") + meta.units,
            rowWidget);
        detail->setObjectName(QStringLiteral("layerDetail_") + meta.id);
        detail->setStyleSheet(QStringLiteral("font-size: 12px; color: #9a8f86;"));
        detail->setToolTip(meta.product);
        rowLayout->addWidget(detail, 1);

        m_rowsLayout->addWidget(rowWidget);

        Row row;
        row.id = meta.id;
        row.toggle = toggle;
        row.detail = detail;
        m_rows.push_back(row);

        connect(toggle, &QCheckBox::toggled, this,
                [this, id = meta.id](bool checked) {
                    emit layerToggled(id, checked);
                });
    }

    for (const Row &row : m_rows)
    {
        const bool visible = visibleIds.contains(row.id);
        const QSignalBlocker blocker(row.toggle);
        row.toggle->setChecked(visible);
    }
}

void LayerLegend::clearRows()
{
    for (const Row &row : m_rows)
    {
        delete row.toggle;
        delete row.detail;
    }
    m_rows.clear();

    // Row container widgets own the layouts and are removed with the rows.
    while (QLayoutItem *item = m_rowsLayout->takeAt(0))
    {
        delete item->widget();
        delete item;
    }
}

int LayerLegend::layerCount() const
{
    return static_cast<int>(m_rows.size());
}

bool LayerLegend::isLayerVisible(const QString &layerId) const
{
    const Row *row = findRow(layerId);
    return row && row->toggle->isChecked();
}

QStringList LayerLegend::visibleLayerIds() const
{
    QStringList out;
    for (const Row &row : m_rows)
    {
        if (row.toggle->isChecked())
            out.append(row.id);
    }
    return out;
}

QCheckBox *LayerLegend::checkbox(const QString &layerId) const
{
    const Row *row = findRow(layerId);
    return row ? row->toggle : nullptr;
}

QLabel *LayerLegend::detailLabel(const QString &layerId) const
{
    const Row *row = findRow(layerId);
    return row ? row->detail : nullptr;
}

void LayerLegend::setLayerVisible(const QString &layerId, bool visible)
{
    const Row *row = findRow(layerId);
    if (!row)
        return;

    const QSignalBlocker blocker(row->toggle);
    row->toggle->setChecked(visible);
}

const LayerLegend::Row *LayerLegend::findRow(const QString &layerId) const
{
    for (const Row &row : m_rows)
    {
        if (row.id == layerId)
            return &row;
    }
    return nullptr;
}
