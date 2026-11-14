// M3c field panel tests: one recommendation, ranked target rows, route
// summary and warnings rendered from the passed model. Runs offscreen
// (QT_QPA_PLATFORM=offscreen set by CTest).

#include "analysis/TargetRanking.h"
#include "routing/TraversePlanner.h"
#include "ui/SidePanel.h"

#include <QLabel>
#include <QScrollArea>
#include <QScrollBar>
#include <QtTest/QTest>

class TestSidePanel : public QObject
{
    Q_OBJECT
private slots:
    void rendersOneRecommendation();
    void showsNoSafeRoute();
    void rendersPassedWarnings();
    void wrappedTextIsNotClipped();
    void safetyAndRouteAreAboveTheFold();

private:
    static RankedTarget makeTarget(const QString &id, const QString &name,
                                   double travelMinutes, double minutesOnTarget,
                                   double valueRate, bool reachable);
};

RankedTarget TestSidePanel::makeTarget(const QString &id, const QString &name,
                                       double travelMinutes,
                                       double minutesOnTarget,
                                       double valueRate, bool reachable)
{
    RankedTarget ranked;
    ranked.target.id = id;
    ranked.target.name = name;
    ranked.target.why = QStringLiteral("why for ") + id;
    ranked.travelMinutes = travelMinutes;
    ranked.target.minutesOnTarget = minutesOnTarget;
    ranked.valueRate = valueRate;
    ranked.reachable = reachable;
    return ranked;
}

void TestSidePanel::rendersOneRecommendation()
{
    SidePanel panel;
    panel.setScienceGoal(QStringLiteral("Characterize boxwork veins."));

    const QVector<RankedTarget> targets = {
        makeTarget(QStringLiteral("T1"), QStringLiteral("Garden City"), 8, 12,
                   0.040, true),
        makeTarget(QStringLiteral("T2"), QStringLiteral("Marias Pass"), 5, 14,
                   0.050, true),
        makeTarget(QStringLiteral("T3"), QStringLiteral("Far Ridge"), 30, 10,
                   0.020, false),
    };
    panel.setRankedTargets(targets);

    Route route;
    route.points = {QPointF(0, 0), QPointF(412, 0)};
    route.distanceM = 412;
    route.timeMin = 21;
    route.maxSlopeDeg = 9;
    route.standoffM = 0;
    route.valid = true;

    RankedTarget primary = targets.at(1);
    primary.target.why = QStringLiteral("Boxwork vein exposed in bedrock.");
    panel.setPrimaryTarget(primary, route);

    QCOMPARE(panel.primaryTargetId(), QStringLiteral("T2"));
    QVERIFY(panel.primaryTargetWhy().contains(QStringLiteral("Boxwork vein")));
    QVERIFY(panel.routeSummaryText().contains(QStringLiteral("412")));
    QVERIFY(panel.routeSummaryText().contains(QStringLiteral("21")));
    QVERIFY(panel.routeSummaryText().contains(QStringLiteral("9")));
    QCOMPARE(panel.rankedTargetCount(), 3);

    auto *t2 = panel.findChild<QLabel *>(QStringLiteral("targetRow_T2"));
    QVERIFY(t2 != nullptr);
    QVERIFY(t2->text().contains(QStringLiteral("Marias")));

    auto *t3 = panel.findChild<QLabel *>(QStringLiteral("targetRow_T3"));
    QVERIFY(t3 != nullptr);
    QVERIFY(t3->text().contains(QStringLiteral("UNREACHABLE")));

    auto *warning = panel.findChild<QLabel *>(QStringLiteral("warningLabel"));
    QVERIFY(warning != nullptr);
    QVERIFY(warning->text().contains(QStringLiteral("20")));
}

void TestSidePanel::showsNoSafeRoute()
{
    SidePanel panel;

    RankedTarget primary = makeTarget(QStringLiteral("T1"),
                                      QStringLiteral("Garden City"), 8, 12,
                                      0.04, false);
    Route route;
    route.valid = false;
    route.warning = QStringLiteral("Goal lies outside the hazard grid.");
    panel.setPrimaryTarget(primary, route);

    QVERIFY(panel.routeSummaryText().contains(QStringLiteral("NO SAFE ROUTE")));
    QVERIFY(panel.routeSummaryText().contains(route.warning));

    auto *walkback = panel.findChild<QLabel *>(QStringLiteral("walkbackNote"));
    QVERIFY(walkback != nullptr);
    QVERIFY(walkback->text().contains(QStringLiteral("pending")));
    QVERIFY(!walkback->text().contains(QStringLiteral("safe"),
                                       Qt::CaseInsensitive));
}

void TestSidePanel::rendersPassedWarnings()
{
    SidePanel panel;
    panel.setWarnings({QStringLiteral("Dust rising: tau 1.1."),
                       QStringLiteral("Comms window closes in 10 min.")});

    QCOMPARE(panel.warningTexts().size(), 2);
    auto *warning = panel.findChild<QLabel *>(QStringLiteral("warningLabel"));
    QVERIFY(warning != nullptr);
    QVERIFY(warning->text().contains(QStringLiteral("Dust rising")));
    QVERIFY(warning->text().contains(QStringLiteral("Comms window")));
}

void TestSidePanel::wrappedTextIsNotClipped()
{
    SidePanel panel;
    panel.setScienceGoal(QStringLiteral(
        "Characterize boxwork fracture-filling sulfate and hydrated mineral "
        "veins at Garden City to reconstruct past fluid chemistry and "
        "habitability."));

    const QVector<RankedTarget> targets = {
        makeTarget(QStringLiteral("T1"),
                   QStringLiteral("Garden City Boxwork Vein Field"), 8, 12,
                   0.040, true),
        makeTarget(QStringLiteral("T2"),
                   QStringLiteral("Marias Pass Stratified Outcrop"), 5, 14,
                   0.050, true),
        makeTarget(QStringLiteral("T3"),
                   QStringLiteral("Far Ridge Fracture Network"), 30, 10,
                   0.020, false),
    };
    panel.setRankedTargets(targets);

    Route route;
    route.points = {QPointF(0, 0), QPointF(412, 0)};
    route.distanceM = 412;
    route.timeMin = 21;
    route.maxSlopeDeg = 9;
    route.standoffM = 0;
    route.valid = true;

    RankedTarget primary = targets.at(0);
    primary.target.why = QStringLiteral(
        "Boxwork vein exposed in bedrock records multiple fluid episodes and "
        "has high biosignature preservation potential.");
    panel.setPrimaryTarget(primary, route);
    panel.setConsumablesText(
        QStringLiteral("O2 68% \u00b7 water 1.4 L \u00b7 battery 74%"));

    panel.setWarnings(
        {QStringLiteral("Dust rising: tau 1.1 reduces visibility."),
         QStringLiteral("Comms window closes in 10 min.")});

    panel.resize(360, 520);
    panel.show();
    QCoreApplication::processEvents();

    const QList<QLabel *> labels = panel.findChildren<QLabel *>();
    for (QLabel *label : labels) {
        if (!label->isVisible() || !label->wordWrap() || label->text().isEmpty())
            continue;
        const int needed = label->heightForWidth(label->width());
        QVERIFY2(label->height() + 1 >= needed,
                 qPrintable(QStringLiteral("%1 clipped: h=%2 hfw=%3 w=%4")
                                .arg(label->objectName())
                                .arg(label->height())
                                .arg(needed)
                                .arg(label->width())));
    }

    auto *t1 = panel.findChild<QLabel *>(QStringLiteral("targetRow_T1"));
    auto *t2 = panel.findChild<QLabel *>(QStringLiteral("targetRow_T2"));
    auto *t3 = panel.findChild<QLabel *>(QStringLiteral("targetRow_T3"));
    QVERIFY(t1 != nullptr);
    QVERIFY(t2 != nullptr);
    QVERIFY(t3 != nullptr);
    QVERIFY(t1->geometry().bottom() < t2->geometry().top());
    QVERIFY(t2->geometry().bottom() < t3->geometry().top());

    auto *scroll = panel.findChild<QScrollArea *>(
        QStringLiteral("sidePanelScroll"));
    QVERIFY(scroll != nullptr);
    QVERIFY(scroll->verticalScrollBar()->maximum() > 0);

    auto *goal = panel.findChild<QLabel *>(QStringLiteral("scienceGoalLabel"));
    QVERIFY(goal != nullptr);
    QVERIFY(goal->isVisible());
    QVERIFY(goal->height() + 1 >= goal->heightForWidth(goal->width()));
}

// Hazard-first ordering (AGENTS.md §7): at a realistic dock height the
// no-go/unknown warning and the route summary must be readable without
// scrolling, with warnings above the route.
void TestSidePanel::safetyAndRouteAreAboveTheFold()
{
    SidePanel panel;
    panel.setScienceGoal(QStringLiteral(
        "Characterize boxwork fracture-filling sulfate and hydrated mineral "
        "veins at Garden City to reconstruct past fluid chemistry and "
        "habitability."));

    const QVector<RankedTarget> targets = {
        makeTarget(QStringLiteral("T1"),
                   QStringLiteral("Garden City Boxwork Vein Field"), 8, 12,
                   0.040, true),
        makeTarget(QStringLiteral("T2"),
                   QStringLiteral("Marias Pass Stratified Outcrop"), 5, 14,
                   0.050, true),
        makeTarget(QStringLiteral("T3"),
                   QStringLiteral("Far Ridge Fracture Network"), 30, 10,
                   0.020, false),
    };
    panel.setRankedTargets(targets);

    Route route;
    route.points = {QPointF(0, 0), QPointF(412, 0)};
    route.distanceM = 412;
    route.timeMin = 21;
    route.maxSlopeDeg = 9;
    route.standoffM = 0;
    route.valid = true;

    RankedTarget primary = targets.at(0);
    primary.target.why = QStringLiteral(
        "Boxwork vein exposed in bedrock records multiple fluid episodes and "
        "has high biosignature preservation potential.");
    panel.setPrimaryTarget(primary, route);
    panel.setConsumablesText(
        QStringLiteral("O2 68% \u00b7 water 1.4 L \u00b7 battery 74%"));
    panel.setWarnings(
        {QStringLiteral("Dust rising: tau 1.1 reduces visibility."),
         QStringLiteral("Comms window closes in 10 min.")});

    panel.resize(300, 600);
    panel.show();
    QCoreApplication::processEvents();

    auto *scroll = panel.findChild<QScrollArea *>(
        QStringLiteral("sidePanelScroll"));
    QVERIFY(scroll != nullptr);
    auto *warning =
        panel.findChild<QLabel *>(QStringLiteral("warningLabel"));
    auto *routeSummary =
        panel.findChild<QLabel *>(QStringLiteral("routeSummaryLabel"));
    QVERIFY(warning != nullptr);
    QVERIFY(routeSummary != nullptr);
    QVERIFY(warning->isVisible());
    QVERIFY(routeSummary->isVisible());

    // Scroll position 0: viewport top shows the first content rows.
    QCOMPARE(scroll->verticalScrollBar()->value(), 0);

    const QWidget *viewport = scroll->viewport();
    const int warningTop =
        warning->mapTo(viewport, QPoint(0, 0)).y();
    const int warningBottom = warningTop + warning->height();
    const int routeTop =
        routeSummary->mapTo(viewport, QPoint(0, 0)).y();
    const int routeBottom = routeTop + routeSummary->height();

    QVERIFY2(warningTop >= 0,
             qPrintable(QStringLiteral("warning starts above viewport: %1")
                            .arg(warningTop)));
    QVERIFY2(warningBottom <= viewport->height() + 1,
             qPrintable(QStringLiteral("warning clipped: bottom=%1 h=%2")
                            .arg(warningBottom)
                            .arg(viewport->height())));
    QVERIFY2(routeBottom <= viewport->height() + 1,
             qPrintable(QStringLiteral("route clipped: bottom=%1 h=%2")
                            .arg(routeBottom)
                            .arg(viewport->height())));
    QVERIFY2(warningTop < routeTop,
             qPrintable(QStringLiteral("warning y=%1 not above route y=%2")
                            .arg(warningTop)
                            .arg(routeTop)));
}

QTEST_MAIN(TestSidePanel)
#include "test_sidepanel.moc"
