#pragma once

#include <QWidget>

// Right-hand field panel (M0 placeholder): section headers only, matching the
// panel layout in PLAN.md §7. Content lands in later milestones.
class SidePanel : public QWidget
{
    Q_OBJECT
public:
    explicit SidePanel(QWidget *parent = nullptr);
};
