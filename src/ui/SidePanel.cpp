#include "ui/SidePanel.h"

#include <QLabel>
#include <QVBoxLayout>

SidePanel::SidePanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    auto *title = new QLabel(tr("FIELD PANEL"), this);
    title->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: bold;"));
    layout->addWidget(title);

    const QStringList sections = {
        tr("SCIENCE GOAL"),
        tr("NEXT TARGET"),
        tr("ROUTE"),
        tr("CONSUMABLES"),
        tr("WARNINGS"),
    };
    for (const QString &section : sections) {
        auto *label = new QLabel(section, this);
        label->setStyleSheet(QStringLiteral("font-size: 14px;"));
        layout->addWidget(label);
    }
    layout->addStretch(1);

    setMinimumWidth(280);
}
