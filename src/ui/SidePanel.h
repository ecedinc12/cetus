#pragma once

#include "analysis/TargetRanking.h"
#include "routing/TraversePlanner.h"

#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QLabel;
class QResizeEvent;
class QScrollArea;
class QVBoxLayout;

// Right-hand field panel: one recommendation at a time, rendered from the
// passed ranking/route model. This widget never ranks targets or plans a
// route; it only displays what the mission state supplies (PLAN.md §7).
class SidePanel : public QWidget
{
    Q_OBJECT
public:
    explicit SidePanel(QWidget *parent = nullptr);

    void setScienceGoal(const QString &goal);
    void setRankedTargets(const QVector<RankedTarget> &targets);
    void setPrimaryTarget(const RankedTarget &primary, const Route &route);
    void setConsumablesText(const QString &text);
    void setWarnings(const QStringList &warnings);

    QString scienceGoalText() const;
    QString primaryTargetId() const;
    QString primaryTargetWhy() const;
    QString routeSummaryText() const;
    QStringList warningTexts() const;
    int rankedTargetCount() const;

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void rebuildTargetRows();
    void applyWrappedText(QLabel *label, const QString &text);
    void updateWrappedHeights();
    int availableTextWidth() const;
    static QString targetRowText(const RankedTarget &ranked);

    QScrollArea *m_scroll = nullptr;
    QWidget *m_content = nullptr;
    QVBoxLayout *m_contentLayout = nullptr;

    QLabel *m_scienceGoalLabel = nullptr;
    QLabel *m_primaryTargetLabel = nullptr;
    QLabel *m_primaryWhyLabel = nullptr;
    QVBoxLayout *m_targetRowsLayout = nullptr;
    QVector<QLabel *> m_targetRows;
    QLabel *m_routeSummaryLabel = nullptr;
    QLabel *m_walkbackNote = nullptr;
    QLabel *m_consumablesLabel = nullptr;
    QLabel *m_warningLabel = nullptr;

    QVector<RankedTarget> m_targets;
    QString m_primaryId;
    QString m_primaryWhy;
    QStringList m_warningTexts;
};
