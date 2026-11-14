#include "ui/SidePanel.h"

#include <QFrame>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QStyle>
#include <QVBoxLayout>

namespace {
// Field-equipment palette: dark panel, light text, amber section headers.
constexpr QColor kPanelBg(28, 22, 18);
constexpr QColor kLightText(245, 240, 232);
constexpr QColor kMutedText(180, 168, 156);
constexpr QColor kAmber(255, 190, 60);
constexpr QColor kWarningRed(120, 24, 20);
constexpr QColor kPrimaryBg(74, 52, 14);

// Conservative line-break width used before the scroll viewport has a size.
constexpr int kFallbackTextWidth = 250;
// Existing target-row padding/minimum height (see rebuildTargetRows).
constexpr int kRowBaseHeight = 34;

QLabel *addSectionHeader(QVBoxLayout *layout, QWidget *parent,
                         const QString &text)
{
    auto *label = new QLabel(text, parent);
    label->setStyleSheet(
        QStringLiteral("font-size: 13px; font-weight: bold; color: #ffbe3c;"
                       " letter-spacing: 1px;"));
    layout->addWidget(label);
    return label;
}
}  // namespace

SidePanel::SidePanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("sidePanel"));
    setStyleSheet(QStringLiteral("QWidget#sidePanel { background: #1c1612; }"));
    setMinimumWidth(340);

    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    m_scroll = new QScrollArea(this);
    m_scroll->setObjectName(QStringLiteral("sidePanelScroll"));
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setStyleSheet(
        QStringLiteral("QScrollArea#sidePanelScroll {"
                       " background: #1c1612; border: none; }"));
    m_scroll->setBackgroundRole(QPalette::Window);
    m_scroll->setAutoFillBackground(true);
    QPalette scrollPalette = m_scroll->palette();
    scrollPalette.setColor(QPalette::Window, kPanelBg);
    m_scroll->setPalette(scrollPalette);
    m_scroll->viewport()->setAutoFillBackground(true);
    QPalette viewportPalette = m_scroll->viewport()->palette();
    viewportPalette.setColor(QPalette::Window, kPanelBg);
    m_scroll->viewport()->setPalette(viewportPalette);
    outerLayout->addWidget(m_scroll);

    m_content = new QWidget();
    m_content->setObjectName(QStringLiteral("sidePanelContent"));
    m_content->setStyleSheet(QStringLiteral(
        "QWidget#sidePanelContent { background: #1c1612; }"));
    m_scroll->setWidget(m_content);

    m_contentLayout = new QVBoxLayout(m_content);
    m_contentLayout->setContentsMargins(16, 4, 16, 10);
    m_contentLayout->setSpacing(4);

    auto *title = new QLabel(tr("FIELD PANEL"), m_content);
    title->setStyleSheet(QStringLiteral(
        "font-size: 20px; font-weight: bold; color: #f5f0e8;"
        " letter-spacing: 2px;"));
    m_contentLayout->addWidget(title);

    // Scripted scenario banner: what step the crew is in and what changed.
    // Field-equipment styling (dark backing, amber border, light text).
    m_scenarioLabel = new QLabel(m_content);
    m_scenarioLabel->setObjectName(QStringLiteral("scenarioContextLabel"));
    m_scenarioLabel->setWordWrap(true);
    m_scenarioLabel->setTextFormat(Qt::RichText);
    m_scenarioLabel->setStyleSheet(
        QStringLiteral("font-size: 12px; color: #f5f0e8;"
                       " background: #241a0c; border: 2px solid #ffbe3c;"
                       " border-radius: 4px; padding: 3px;"));
    m_contentLayout->addWidget(m_scenarioLabel);

    addSectionHeader(m_contentLayout, m_content, tr("SCIENCE GOAL"));
    m_scienceGoalLabel = new QLabel(tr("No science goal set."), m_content);
    m_scienceGoalLabel->setObjectName(QStringLiteral("scienceGoalLabel"));
    m_scienceGoalLabel->setWordWrap(true);
    m_scienceGoalLabel->setStyleSheet(
        QStringLiteral("font-size: 14px; color: #f5f0e8;"));
    m_contentLayout->addWidget(m_scienceGoalLabel);

    // Hazard-first ordering (AGENTS.md §7): warnings, route and target sit
    // above the fold at demo dock height; ranked list is the drill-down below.
    addSectionHeader(m_contentLayout, m_content, tr("WARNINGS"));
    m_warningLabel = new QLabel(m_content);
    m_warningLabel->setObjectName(QStringLiteral("warningLabel"));
    m_warningLabel->setWordWrap(true);
    m_warningLabel->setStyleSheet(
        QStringLiteral("font-size: 12px; font-weight: bold; color: #ffd9d0;"
                       " background: #781814; border: 1px solid #b3302a;"
                       " border-radius: 4px; padding: 4px;"));
    m_contentLayout->addWidget(m_warningLabel);

    addSectionHeader(m_contentLayout, m_content, tr("ROUTE"));
    m_routeSummaryLabel = new QLabel(tr("NO SAFE ROUTE"), m_content);
    m_routeSummaryLabel->setObjectName(QStringLiteral("routeSummaryLabel"));
    m_routeSummaryLabel->setWordWrap(true);
    m_routeSummaryLabel->setStyleSheet(
        QStringLiteral("font-size: 15px; font-weight: bold; color: #7fe6ff;"));
    m_contentLayout->addWidget(m_routeSummaryLabel);

    // Walkback status (M4). Until a plan is checked it says so; it never
    // implies the route is safe.
    m_walkbackNote = new QLabel(tr("Walkback: pending (no plan to check)"),
                                m_content);
    m_walkbackNote->setObjectName(QStringLiteral("walkbackNote"));
    m_walkbackNote->setWordWrap(true);
    m_walkbackNote->setStyleSheet(
        QStringLiteral("font-size: 12px; color: #b4a89c;"));
    m_contentLayout->addWidget(m_walkbackNote);

    addSectionHeader(m_contentLayout, m_content, tr("NEXT TARGET"));
    m_primaryTargetLabel = new QLabel(tr("NO TARGET"), m_content);
    m_primaryTargetLabel->setObjectName(QStringLiteral("primaryTargetLabel"));
    m_primaryTargetLabel->setWordWrap(true);
    m_primaryTargetLabel->setStyleSheet(
        QStringLiteral("font-size: 18px; font-weight: bold; color: #ff8c00;"));
    m_contentLayout->addWidget(m_primaryTargetLabel);

    m_primaryWhyLabel = new QLabel(tr("No reachable target."), m_content);
    m_primaryWhyLabel->setObjectName(QStringLiteral("primaryWhyLabel"));
    m_primaryWhyLabel->setWordWrap(true);
    m_primaryWhyLabel->setStyleSheet(
        QStringLiteral("font-size: 13px; color: #d8cec2;"));
    m_contentLayout->addWidget(m_primaryWhyLabel);

    m_collectSampleButton = new QPushButton(tr("COLLECT SAMPLE"), m_content);
    m_collectSampleButton->setObjectName(
        QStringLiteral("collectSampleButton"));
    m_collectSampleButton->setMinimumHeight(44);
    m_collectSampleButton->setEnabled(false);
    m_collectSampleButton->setToolTip(
        tr("Appends a field note to the session log; no physical sample is "
           "claimed."));
    m_collectSampleButton->setStyleSheet(
        QStringLiteral("QPushButton { font-size: 16px; font-weight: bold;"
                       " color: #1c1612; background: #ffbe3c;"
                       " border: 2px solid #ffd97a; border-radius: 6px;"
                       " padding: 8px; }"
                       "QPushButton:disabled { color: #6b5b45;"
                       " background: #4a3d33; border: 2px solid #6b5b45; }"));
    m_contentLayout->addWidget(m_collectSampleButton);
    connect(m_collectSampleButton, &QPushButton::clicked, this,
            &SidePanel::collectSampleRequested);

    m_latestSampleLabel = new QLabel(tr("No samples logged."), m_content);
    m_latestSampleLabel->setObjectName(QStringLiteral("latestSampleLabel"));
    m_latestSampleLabel->setWordWrap(true);
    m_latestSampleLabel->setStyleSheet(
        QStringLiteral("font-size: 13px; color: #7fe6ff;"));
    m_contentLayout->addWidget(m_latestSampleLabel);

    m_sampleErrorLabel = new QLabel(m_content);
    m_sampleErrorLabel->setObjectName(QStringLiteral("sampleErrorLabel"));
    m_sampleErrorLabel->setWordWrap(true);
    m_sampleErrorLabel->setStyleSheet(
        QStringLiteral("font-size: 13px; font-weight: bold; color: #ffd9d0;"
                       " background: #781814; border: 1px solid #b3302a;"
                       " border-radius: 4px; padding: 6px;"));
    m_sampleErrorLabel->hide();
    m_contentLayout->addWidget(m_sampleErrorLabel);

    addSectionHeader(m_contentLayout, m_content, tr("CONSUMABLES"));
    m_consumablesLabel = new QLabel(tr("Not linked yet (M4)"), m_content);
    m_consumablesLabel->setObjectName(QStringLiteral("consumablesLabel"));
    m_consumablesLabel->setWordWrap(true);
    m_consumablesLabel->setStyleSheet(
        QStringLiteral("font-size: 13px; color: #f5f0e8;"));
    m_contentLayout->addWidget(m_consumablesLabel);

    addSectionHeader(m_contentLayout, m_content, tr("RANKED TARGETS"));
    m_targetRowsLayout = new QVBoxLayout();
    m_targetRowsLayout->setContentsMargins(0, 0, 0, 0);
    m_targetRowsLayout->setSpacing(6);
    m_contentLayout->addLayout(m_targetRowsLayout);

    m_contentLayout->addStretch(1);

    setWarnings(QStringList());
}

void SidePanel::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateWrappedHeights();
}

int SidePanel::availableTextWidth() const
{
    int viewportWidth = 0;
    if (m_scroll)
        viewportWidth = m_scroll->viewport()->width();
    if (viewportWidth <= 0)
        viewportWidth = width();
    if (viewportWidth <= 0)
        return kFallbackTextWidth;

    int reserve = 0;
    if (m_contentLayout) {
        const QMargins margins = m_contentLayout->contentsMargins();
        reserve += margins.left() + margins.right();
    }
    // Always reserve the vertical scrollbar: once it appears the viewport
    // narrows, and a taller wrapped label is safer than a clipped one.
    if (m_scroll) {
        const int extent = m_scroll->style()->pixelMetric(
            QStyle::PM_ScrollBarExtent, nullptr, m_scroll);
        if (extent > 0)
            reserve += extent;
    }

    const int available = viewportWidth - reserve;
    return available > kFallbackTextWidth ? available : kFallbackTextWidth;
}

void SidePanel::applyWrappedText(QLabel *label, const QString &text)
{
    if (!label)
        return;
    label->setText(text);
    updateWrappedHeights();
}

void SidePanel::updateWrappedHeights()
{
    const int width = availableTextWidth();

    const auto setLabelMinimum = [width](QLabel *label, int baseHeight) {
        if (!label)
            return;
        int minimum = baseHeight;
        if (label->wordWrap() && !label->text().isEmpty()) {
            const int needed = label->heightForWidth(width);
            if (needed > minimum)
                minimum = needed;
        }
        label->setMinimumHeight(minimum);
    };

    setLabelMinimum(m_scenarioLabel, 0);
    setLabelMinimum(m_scienceGoalLabel, 0);
    setLabelMinimum(m_primaryTargetLabel, 0);
    setLabelMinimum(m_primaryWhyLabel, 0);
    setLabelMinimum(m_latestSampleLabel, 0);
    setLabelMinimum(m_sampleErrorLabel, 0);
    for (QLabel *row : m_targetRows)
        setLabelMinimum(row, kRowBaseHeight);
    setLabelMinimum(m_routeSummaryLabel, 0);
    setLabelMinimum(m_walkbackNote, 0);
    setLabelMinimum(m_consumablesLabel, 0);
    setLabelMinimum(m_warningLabel, 0);
}

void SidePanel::setScenarioContext(const QString &stepId,
                                   const QString &title,
                                   const QString &detail, int stepIndex,
                                   int stepCount)
{
    if (!m_scenarioLabel)
        return;
    const QString heading =
        QStringLiteral("SCENARIO %1 \u00b7 %2 \u00b7 %3/%4")
            .arg(stepId, title)
            .arg(stepIndex + 1)
            .arg(stepCount);
    m_scenarioLabel->setText(QStringLiteral("<b>%1</b><br>%2")
                                 .arg(heading.toHtmlEscaped(),
                                      detail.toHtmlEscaped()));
    updateWrappedHeights();
}

void SidePanel::setScienceGoal(const QString &goal)
{
    applyWrappedText(m_scienceGoalLabel, goal);
}

void SidePanel::setRankedTargets(const QVector<RankedTarget> &targets)
{
    m_targets = targets;
    rebuildTargetRows();
}

void SidePanel::setPrimaryTarget(const RankedTarget &primary, const Route &route)
{
    m_primaryId = primary.target.id;
    m_primaryWhy = primary.target.why;

    if (primary.target.id.isEmpty()) {
        applyWrappedText(m_primaryTargetLabel, tr("NO TARGET"));
    } else {
        applyWrappedText(m_primaryTargetLabel,
                         QStringLiteral("%1 \u00b7 %2")
                             .arg(primary.target.id, primary.target.name));
    }
    applyWrappedText(m_primaryWhyLabel, primary.target.why);

    if (route.valid) {
        const QString summary =
            QStringLiteral("ROUTE %1 m \u00b7 ~%2 min \u00b7 max slope %3\u00b0"
                           " \u00b7 standoff %4 m")
                .arg(route.distanceM, 0, 'f', 0)
                .arg(route.timeMin, 0, 'f', 0)
                .arg(route.maxSlopeDeg, 0, 'f', 0)
                .arg(route.standoffM, 0, 'f', 0);
        applyWrappedText(m_routeSummaryLabel, summary);
    } else {
        QString summary = tr("NO SAFE ROUTE");
        if (!route.warning.isEmpty())
            summary += QStringLiteral(" \u2014 ") + route.warning;
        applyWrappedText(m_routeSummaryLabel, summary);
    }

    // The primary row is now known; restyle the existing rows.
    rebuildTargetRows();
}

void SidePanel::setConsumablesText(const QString &text)
{
    applyWrappedText(m_consumablesLabel, text);
}

void SidePanel::setWalkbackText(const QString &text)
{
    applyWrappedText(m_walkbackNote, text);
}

void SidePanel::setRouteSummaryText(const QString &text)
{
    applyWrappedText(m_routeSummaryLabel, text);
}

void SidePanel::setCollectTarget(const QString &targetId)
{
    m_collectTargetId = targetId;
    const bool enabled = !targetId.isEmpty();
    if (!m_collectSampleButton)
        return;
    m_collectSampleButton->setEnabled(enabled);
    m_collectSampleButton->setText(
        enabled ? tr("COLLECT SAMPLE AT %1").arg(targetId)
                : tr("COLLECT SAMPLE"));
}

void SidePanel::setLatestSample(const QString &sampleId,
                                const QString &summary, int total)
{
    m_latestSampleId = sampleId;
    if (!m_latestSampleLabel)
        return;
    if (sampleId.isEmpty()) {
        applyWrappedText(m_latestSampleLabel, tr("No samples logged."));
        return;
    }

    QString text = sampleId;
    if (!summary.isEmpty())
        text += QStringLiteral(" \u00b7 ") + summary;
    text += QStringLiteral(" \u00b7 %1 logged").arg(total);
    applyWrappedText(m_latestSampleLabel, text);
}

void SidePanel::revealLatestSample()
{
    if (!m_scroll || !m_latestSampleLabel)
        return;
    m_scroll->ensureWidgetVisible(m_latestSampleLabel, 0, 24);
}

void SidePanel::setSampleLogError(const QString &error)
{
    m_sampleErrorText = error;
    if (!m_sampleErrorLabel)
        return;
    if (error.isEmpty()) {
        m_sampleErrorLabel->clear();
        m_sampleErrorLabel->hide();
        return;
    }
    applyWrappedText(m_sampleErrorLabel,
                     QStringLiteral("SAMPLE LOG: %1").arg(error));
    m_sampleErrorLabel->show();
}

QString SidePanel::collectTargetId() const
{
    return m_collectTargetId;
}

QString SidePanel::latestSampleId() const
{
    return m_latestSampleId;
}

QString SidePanel::sampleErrorText() const
{
    return m_sampleErrorText;
}

void SidePanel::setWarnings(const QStringList &warnings)
{
    m_warningTexts = warnings;
    if (warnings.isEmpty()) {
        applyWrappedText(m_warningLabel,
                         tr("! No-go: slope > 20\u00b0 or unknown data"));
        return;
    }

    QStringList lines;
    lines.reserve(warnings.size());
    for (const QString &warning : warnings)
        lines.append(QStringLiteral("! ") + warning);
    applyWrappedText(m_warningLabel, lines.join(QLatin1Char('\n')));
}

QString SidePanel::scenarioContextText() const
{
    return m_scenarioLabel ? m_scenarioLabel->text() : QString();
}

QString SidePanel::scienceGoalText() const
{
    return m_scienceGoalLabel->text();
}

QString SidePanel::primaryTargetId() const
{
    return m_primaryId;
}

QString SidePanel::primaryTargetWhy() const
{
    return m_primaryWhy;
}

QString SidePanel::routeSummaryText() const
{
    return m_routeSummaryLabel->text();
}

QString SidePanel::walkbackText() const
{
    return m_walkbackNote->text();
}

QStringList SidePanel::warningTexts() const
{
    return m_warningTexts;
}

int SidePanel::rankedTargetCount() const
{
    return static_cast<int>(m_targets.size());
}

QString SidePanel::targetRowText(const RankedTarget &ranked)
{
    const QString head = QStringLiteral("%1 \u00b7 %2")
                             .arg(ranked.target.id, ranked.target.name);
    const QString travel =
        QStringLiteral("%1 min travel")
            .arg(ranked.travelMinutes, 0, 'f', 0);
    const QString onTarget =
        QStringLiteral("%1 min on target")
            .arg(ranked.target.minutesOnTarget, 0, 'f', 0);

    if (!ranked.reachable) {
        return QStringLiteral("%1 \u2014 %2 \u00b7 %3 \u00b7 UNREACHABLE")
            .arg(head, travel, onTarget);
    }

    if (ranked.walkbackFailed) {
        return QStringLiteral("%1 \u2014 %2 \u00b7 %3 \u00b7 WALKBACK FAIL")
            .arg(head, travel, onTarget);
    }

    const QString rate =
        QStringLiteral("%1/min").arg(ranked.valueRate, 0, 'f', 3);
    return QStringLiteral("%1 \u2014 %2 \u00b7 %3 \u00b7 %4")
        .arg(head, travel, onTarget, rate);
}

void SidePanel::rebuildTargetRows()
{
    m_targetRows.clear();
    while (QLayoutItem *item = m_targetRowsLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    for (const RankedTarget &ranked : m_targets) {
        auto *row = new QLabel(targetRowText(ranked), m_content);
        row->setObjectName(QStringLiteral("targetRow_") + ranked.target.id);
        row->setWordWrap(true);
        row->setMinimumHeight(kRowBaseHeight);
        row->setContentsMargins(8, 4, 8, 4);

        QString style =
            QStringLiteral("font-size: 13px; color: #f5f0e8;"
                           " border: 1px solid #4a3d33; border-radius: 4px;"
                           " padding: 6px;");
        if (!ranked.reachable || ranked.walkbackFailed) {
            style += QStringLiteral(" color: #d9a49e;");
        }
        if (ranked.target.id == m_primaryId) {
            style += QStringLiteral(" border: 2px solid #ffbe3c;"
                                    " background: #4a340e;"
                                    " font-weight: bold; color: #ffe1a3;");
        }
        row->setStyleSheet(style);
        m_targetRowsLayout->addWidget(row);
        m_targetRows.append(row);
    }

    updateWrappedHeights();
}
