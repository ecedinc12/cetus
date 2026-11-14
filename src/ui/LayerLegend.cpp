#include "ui/LayerLegend.h"

#include "data/DatasetRegistry.h"

#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QVBoxLayout>

LayerLegend::LayerLegend(QWidget *parent)
    : QWidget(parent)
{
    auto *outer = new QHBoxLayout(this);
    outer->setContentsMargins(12, 10, 12, 10);
    outer->setSpacing(12);

    // Left column: the layer toggles, unchanged from earlier milestones.
    auto *layersLayout = new QVBoxLayout();
    layersLayout->setContentsMargins(0, 0, 0, 0);
    layersLayout->setSpacing(8);

    auto *title = new QLabel(tr("LAYERS"), this);
    title->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: bold;"));
    layersLayout->addWidget(title);

    auto *hint = new QLabel(
        tr("check to show \u00b7 source \u00b7 resolution \u00b7 units"), this);
    hint->setStyleSheet(QStringLiteral("font-size: 11px; color: #9a8f86;"));
    layersLayout->addWidget(hint);

    m_rowsLayout = new QVBoxLayout();
    m_rowsLayout->setContentsMargins(0, 4, 0, 0);
    m_rowsLayout->setSpacing(6);
    layersLayout->addLayout(m_rowsLayout);
    layersLayout->addStretch(1);

    outer->addLayout(layersLayout, 1);

    auto *separator = new QFrame(this);
    separator->setFrameShape(QFrame::VLine);
    separator->setFrameShadow(QFrame::Plain);
    separator->setStyleSheet(QStringLiteral("color: #4a403a;"));
    outer->addWidget(separator);

    // Right column: compact key for the derived overlays and the rotated
    // HiRISE footprint. Bounded width so the layer rows stay usable at
    // 1280x800.
    auto *keyLayout = new QVBoxLayout();
    keyLayout->setContentsMargins(0, 0, 0, 0);
    keyLayout->setSpacing(6);

    auto *keyTitle = new QLabel(tr("MAP KEY"), this);
    keyTitle->setStyleSheet(
        QStringLiteral("font-size: 16px; font-weight: bold;"));
    keyLayout->addWidget(keyTitle);

    m_mapKeyNote = new QLabel(this);
    m_mapKeyNote->setObjectName(QStringLiteral("mapKeyNote"));
    m_mapKeyNote->setTextFormat(Qt::RichText);
    m_mapKeyNote->setWordWrap(true);
    m_mapKeyNote->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_mapKeyNote->setMinimumWidth(360);
    m_mapKeyNote->setMaximumWidth(460);
    // Never let the key force or exceed the bottom dock: it wraps instead.
    m_mapKeyNote->setMinimumHeight(0);
    m_mapKeyNote->setSizePolicy(QSizePolicy::Preferred,
                                QSizePolicy::MinimumExpanding);
    m_mapKeyNote->setStyleSheet(
        QStringLiteral("font-size: 11px; color: #d6cdc3;"));
    m_mapKeyNote->setText(
        tr("<p style=\"margin:0\"><b>HAZARD</b> — derived 10 m grid from "
           "HiRISE slope. "
           "<span style=\"color:#dc1e14\">NO-GO</span> &gt;20° · "
           "<span style=\"color:#9b59c0\">UNKNOWN</span> no data = no-go · "
           "<span style=\"color:#ffaa00\">CAUTION</span> 10–20°</p>"
           "<p style=\"margin:0\"><b style=\"color:#00dcff\">ROUTE</b> "
           "solid · "
           "<b style=\"color:#00dcff\">PLAN</b> dashed · "
           "<b style=\"color:#50d25a\">START</b> · "
           "<b style=\"color:#ffbe3c\">TARGET</b> · "
           "<b style=\"color:#ff8c00\">NEXT</b></p>"
           "<p style=\"margin:0\"><b>DATA NOTE</b> — HiRISE strip is "
           "rotated in the north-up Mars grid; black wedges = no data; "
           "arrow = grid north.</p>"));
    keyLayout->addWidget(m_mapKeyNote);
    keyLayout->addStretch(1);

    outer->addLayout(keyLayout);
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

QString LayerLegend::mapKeyText() const
{
    return m_mapKeyNote ? m_mapKeyNote->text() : QString();
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
