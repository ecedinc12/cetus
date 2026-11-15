#include "ui/MainWindow.h"

#include "data/RasterLayer.h"
#include "analysis/Radiation.h"
#include "data/SiteConfig.h"
#include "routing/Walkback.h"
#include "app/SampleCollector.h"
#include "app/SampleLog.h"
#include "ui/LayerLegend.h"
#include "ui/MapView.h"
#include "ui/SidePanel.h"

#include <QComboBox>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QLineF>
#include <QPushButton>
#include <QShortcut>
#include <cmath>
#include <QVBoxLayout>
#include <algorithm>
#include <QDebug>
#include <QDockWidget>
#include <QLabel>
#include <QShowEvent>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>

namespace {
// [design choice] The science goal is a mission design statement, not a
// measurement from the loaded data. Kept in one place so the panel wording
// cannot drift.
constexpr const char *kScienceGoal =
    "Gale Crater, Mount Sharp: characterize fracture-filling mineral phases "
    "and reconstruct past fluid chemistry.";

// Order-independent key for a target pair (the traverse cost is symmetric).
QString pairKey(const QString &a, const QString &b)
{
    return a < b ? a + QLatin1Char('|') + b : b + QLatin1Char('|') + a;
}
// [design choice] User-placed targets get the top science value so a safe one
// is recommended first, and a default work time at the target.
constexpr double kUserTargetValue = 1.0;
constexpr double kUserTargetMinutes = 15.0;
// [design choice] Cap on user targets: each one costs a path search to every
// other target.
constexpr int kMaxUserTargets = 6;
// [design choice] Two points closer than this (one hazard cell) are the same
// point.
constexpr double kMinPointSpacingM = 10.0;

// Radiation line (third data kind). `evaMin` < 0 means no plan: show the rate.
QString radiationText(double evaMin)
{
    QString text = QStringLiteral(
        "RADIATION (MSL RAD, NASA): %1 +/- %2 mSv/day surface dose rate")
        .arg(Radiation::kDoseRateMsvPerDay, 0, 'f', 2)
        .arg(Radiation::kDoseRateSigmaMsvPerDay, 0, 'f', 2);
    if (evaMin >= 0.0) {
        text += QStringLiteral(
            "; est. %1 mSv over the planned %2 min EVA (no suit shielding "
            "model).")
            .arg(Radiation::evaDoseMsv(evaMin), 0, 'f', 2)
            .arg(evaMin, 0, 'f', 0);
    } else {
        text += QLatin1Char('.');
    }
    return text;
}
}  // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_mapView(new MapView(this))
    , m_sidePanel(new SidePanel(this))
{
    setWindowTitle(tr("Cetus — Mars Surface Field Tool"));
    resize(1280, 800);

    setCentralWidget(m_mapView);
    // The side panel and bottom legend leave the central map narrow; without
    // a floor the map can collapse to a letterbox when the window is short.
    // Keep enough height for a usable field view of a portrait raster.
    constexpr int kMapMinimumHeight = 500;
    m_mapView->setMinimumHeight(kMapMinimumHeight);

    auto *panelDock = new QDockWidget(tr("Field Panel"), this);
    panelDock->setObjectName(QStringLiteral("fieldPanelDock"));
    panelDock->setWidget(m_sidePanel);
    panelDock->setFeatures(QDockWidget::DockWidgetMovable);
    addDockWidget(Qt::RightDockWidgetArea, panelDock);

    // Bottom dock: one glove-friendly toggle row per prepared raster, each
    // naming its source, resolution and units.
    auto *legendDock = new QDockWidget(tr("Layers"), this);
    legendDock->setObjectName(QStringLiteral("layerLegendDock"));
    m_legend = new LayerLegend(legendDock);
    legendDock->setWidget(m_legend);
    addDockWidget(Qt::BottomDockWidgetArea, legendDock);

    buildRouteDock();

    // Map toolbar: fit / zoom controls only; layer choice lives in the legend.
    QToolBar *toolBar = addToolBar(tr("Map"));
    toolBar->setObjectName(QStringLiteral("mapToolBar"));
    toolBar->setMovable(false);

    auto *fitButton = new QToolButton(this);
    fitButton->setObjectName(QStringLiteral("fitButton"));
    fitButton->setText(tr("Fit"));
    toolBar->addWidget(fitButton);

    auto *zoomInButton = new QToolButton(this);
    zoomInButton->setObjectName(QStringLiteral("zoomInButton"));
    zoomInButton->setText(QStringLiteral("+"));
    toolBar->addWidget(zoomInButton);

    auto *zoomOutButton = new QToolButton(this);
    zoomOutButton->setObjectName(QStringLiteral("zoomOutButton"));
    zoomOutButton->setText(QStringLiteral("-"));
    toolBar->addWidget(zoomOutButton);

    // Overlay toggles: the hazard grid and the target markers are drawn by
    // default (hazard-first, AGENTS.md §7) but can be hidden to see the bare
    // terrain. Hiding them does not change the plan.
    toolBar->addSeparator();
    auto *hazardButton = new QToolButton(this);
    hazardButton->setObjectName(QStringLiteral("hazardButton"));
    hazardButton->setText(tr("Hazard"));
    hazardButton->setCheckable(true);
    hazardButton->setChecked(true);
    hazardButton->setToolTip(
        tr("Hazard overlay: amber = slope 10-20 deg, red = no-go (slope > "
           "20 deg or unknown data)"));
    toolBar->addWidget(hazardButton);

    auto *targetsButton = new QToolButton(this);
    targetsButton->setObjectName(QStringLiteral("targetsButton"));
    targetsButton->setText(tr("Targets"));
    targetsButton->setCheckable(true);
    targetsButton->setChecked(true);
    targetsButton->setToolTip(
        tr("Science target markers: large orange = recommended, small amber "
           "= other reachable target, red ring = no route or walkback fail"));
    toolBar->addWidget(targetsButton);

    // One header control for the scripted demo scenario (PLAN.md §6).
    toolBar->addSeparator();
    m_advanceButton = new QToolButton(this);
    m_advanceButton->setObjectName(QStringLiteral("advanceButton"));
    m_advanceButton->setText(tr("Advance scenario"));
    m_advanceButton->setEnabled(false);
    toolBar->addWidget(m_advanceButton);

    // Free route: pick START and targets on the map, independent of the
    // scenario (which stays available when the mode is switched off).
    m_freeButton = new QToolButton(this);
    m_freeButton->setObjectName(QStringLiteral("freeRouteButton"));
    m_freeButton->setText(tr("Free route"));
    m_freeButton->setCheckable(true);
    m_freeButton->setEnabled(false);
    m_freeButton->setToolTip(
        tr("Plan from your own points: double-click the map to set the START, "
           "then double-click to add targets. Scenario S0-S4 is kept."));
    toolBar->addWidget(m_freeButton);

    // User targets: double-click the map to add one; this removes them.
    m_clearUserButton = new QToolButton(this);
    m_clearUserButton->setObjectName(QStringLiteral("clearUserTargetsButton"));
    m_clearUserButton->setText(tr("Clear my targets"));
    m_clearUserButton->setToolTip(
        tr("Double-click the map to add a target (max %1). Removes them."
           ).arg(kMaxUserTargets));
    m_clearUserButton->setEnabled(false);
    toolBar->addWidget(m_clearUserButton);

    // Always-visible twin of the panel collect button: at the demo window size
    // the panel's own button can sit below the fold, so the everyday action
    // also lives in the map toolbar.
    m_collectToolbarButton = new QToolButton(this);
    m_collectToolbarButton->setObjectName(
        QStringLiteral("collectSampleToolbarButton"));
    m_collectToolbarButton->setText(tr("COLLECT SAMPLE"));
    m_collectToolbarButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_collectToolbarButton->setMinimumHeight(32);
    m_collectToolbarButton->setEnabled(false);
    m_collectToolbarButton->setToolTip(
        tr("Appends a field note to the session log; no physical sample is "
           "claimed."));
    m_collectToolbarButton->setStyleSheet(
        QStringLiteral("QToolButton { min-height: 32px; font-size: 14px;"
                       " font-weight: bold; color: #1c1612;"
                       " background: #ffbe3c; border: 2px solid #ffd97a;"
                       " border-radius: 6px; padding: 4px 12px; }"
                       "QToolButton:disabled { color: #6b5b45;"
                       " background: #4a3d33; border: 2px solid #6b5b45; }"));
    toolBar->addWidget(m_collectToolbarButton);

    // Permanent cursor readout (projected metres) in the status bar.
    m_cursorLabel = new QLabel(this);
    m_cursorLabel->setObjectName(QStringLiteral("cursorLabel"));
    statusBar()->addPermanentWidget(m_cursorLabel);

    // Load the prepared rasters. No modal dialog: the app stays usable and
    // the failure is announced loudly in the status bar (AGENTS.md §6).
    QString error;
    if (!m_registry.loadPrepared(&error)) {
        m_loadError = error;
        qCritical().noquote() << "Cetus: raster load failed:" << error;
    } else {
        m_mapView->setRegistry(&m_registry);
        m_legend->setRegistry(&m_registry, m_mapView->visibleLayerIds());
    }
    if (layersLoaded()) {
        loadMission();
        updateAdvanceButton();
        if (m_scenarioLoaded)
            openSampleLog();
    }
    updateStatus();

    connect(m_sidePanel, &SidePanel::collectSampleRequested, this,
            &MainWindow::collectSample);
    connect(m_legend, &LayerLegend::layerToggled, this,
            [this](const QString &layerId, bool visible) {
                m_mapView->setLayerVisible(layerId, visible);
            });
    connect(m_mapView, &MapView::visibleLayersChanged, this,
            &MainWindow::updateStatus);
    connect(fitButton, &QToolButton::clicked, m_mapView,
            &MapView::fitToScene);
    connect(zoomInButton, &QToolButton::clicked, m_mapView, &MapView::zoomIn);
    connect(zoomOutButton, &QToolButton::clicked, m_mapView,
            &MapView::zoomOut);
    connect(hazardButton, &QToolButton::toggled, m_mapView,
            &MapView::setHazardVisible);
    connect(targetsButton, &QToolButton::toggled, m_mapView,
            &MapView::setTargetsVisible);
    connect(m_advanceButton, &QToolButton::clicked, this,
            &MainWindow::advanceScenario);
    connect(m_collectToolbarButton, &QToolButton::clicked, this,
            &MainWindow::collectSample);
    connect(m_mapView, &MapView::targetRequested, this,
            &MainWindow::handleMapDoubleClick);
    connect(m_freeButton, &QToolButton::toggled, this,
            &MainWindow::setFreeMode);
    connect(m_clearUserButton, &QToolButton::clicked, this,
            &MainWindow::clearUserTargets);
    connect(m_mapView, &MapView::cursorMoved, this,
            [this](double eastingM, double northingM) {
                m_cursorLabel->setText(
                    QStringLiteral("E %1 m · N %2 m")
                        .arg(eastingM, 0, 'f', 0)
                        .arg(northingM, 0, 'f', 0));
            });
}

MainWindow::~MainWindow() = default;

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    // MapView fits once during its own showEvent, but the bottom legend dock
    // only reaches its final height after the docking layout settles. Refit
    // once the event loop has caught up so the opening view matches the real
    // widget size.
    if (!m_didInitialFitAfterLayout && layersLoaded()) {
        m_didInitialFitAfterLayout = true;
        QTimer::singleShot(0, this, [this]() {
            if (m_route.valid)
                m_mapView->fitToRoute();
            else
                m_mapView->fitToScene();
        });
    }
}

void MainWindow::loadMission()
{
    m_missionError.clear();
    m_scenarioLoaded = false;
    m_missionReady = false;
    m_stepIndex = 0;
    m_state = MissionState();

    QString error;
    if (!MissionScenario::loadFromFile(SiteConfig::scenarioPath(), &m_scenario,
                                       &error)) {
        m_missionError = error;
        qCritical().noquote() << "Cetus: mission scenario load failed:"
                              << error;
        return;
    }

    const RasterLayer *slope = m_registry.layer(QStringLiteral("hirise_slope"));
    if (!slope) {
        m_missionError =
            QStringLiteral("hazard layer hirise_slope is not in the registry");
        qCritical().noquote() << "Cetus: hazard build failed:" << m_missionError;
        return;
    }
    if (!m_hazard.build(*slope, HazardMap::kCellSizeM, &error)) {
        m_missionError = error;
        qCritical().noquote() << "Cetus: hazard build failed:" << error;
        return;
    }

    // Terrain does not change between scenario steps, so the routes are
    // planned once here.
    rebuildPlanning();
    m_missionReady = true;
    if (m_freeButton)
        m_freeButton->setEnabled(true);

    replan(true);
}

void MainWindow::rebuildPlanning()
{
    m_routeById.clear();
    m_travelById.clear();
    m_routeById.reserve(m_scenario.targets.size());
    m_travelById.reserve(m_scenario.targets.size());
    for (const ScienceTarget &target : m_scenario.targets) {
        const Route planned =
            TraversePlanner::planToTarget(m_hazard, m_scenario.crewStart,
                                          target);
        m_routeById.insert(target.id, planned);
        // Negative means unreachable; never fabricate a travel time.
        m_travelById.insert(target.id,
                            planned.valid ? planned.timeMin : -1.0);
    }
    // Target-to-target leg times for the greedy multi-target plan. Edge costs
    // are symmetric, so one plan per unordered pair serves both directions.
    m_legByPair.clear();
    m_legRouteByPair.clear();
    for (int i = 0; i < m_scenario.targets.size(); ++i) {
        for (int j = i + 1; j < m_scenario.targets.size(); ++j) {
            const ScienceTarget &from = m_scenario.targets.at(i);
            const ScienceTarget &to = m_scenario.targets.at(j);
            const Route leg =
                TraversePlanner::planToTarget(m_hazard, from.position(), to);
            m_legByPair.insert(pairKey(from.id, to.id),
                               leg.valid ? leg.timeMin : -1.0);
            m_legRouteByPair.insert(pairKey(from.id, to.id), leg);
        }
    }
}

bool MainWindow::freeMode() const
{
    return m_freeMode;
}

void MainWindow::handleMapDoubleClick(double eastingM, double northingM)
{
    if (!m_missionReady)
        return;
    m_notice.clear();
    if (m_freeMode && !m_freeStartSet) {
        if (!m_hazard.contains(eastingM, northingM) ||
            m_hazard.isNoGoAt(eastingM, northingM)) {
            notify(tr("START rejected: that point is outside the map or on "
                      "no-go/unknown terrain. Pick a safe point."));
            return;
        }
        m_scenario.crewStart = QPointF(eastingM, northingM);
        m_freeStartSet = true;
        m_mapView->setCrewStart(m_scenario.crewStart);
        replan(false);
        updateStatus();
        return;
    }
    addUserTarget(eastingM, northingM);
}

void MainWindow::setFreeMode(bool on)
{
    if (!m_missionReady || on == m_freeMode)
        return;
    m_freeMode = on;
    m_notice.clear();
    if (on) {
        m_savedScenario = m_scenario;
        m_scenario.targets.clear();
        m_freeStartSet = false;
        m_userTargetSerial = 0;
        // Free mode runs on the nominal state; the scripted events are off.
        m_stepIndex = 0;
        const ScenarioStep &s0 = ScenarioSteps::at(0);
        m_state.tau = s0.tau;
        m_state.timeBudgetMin = effectiveBudget(s0);
        m_state.commsClosed = s0.commsClosed;
        m_state.commsOfflineMin = s0.commsOfflineMin;
        m_routeById.clear();
        m_travelById.clear();
        m_legByPair.clear();
        m_legRouteByPair.clear();
        m_mapView->clearCrewStart();
        m_clearUserButton->setEnabled(true);
    } else {
        m_scenario = m_savedScenario;
        m_userTargetSerial = 0;
        rebuildPlanning();
        m_mapView->setCrewStart(m_scenario.crewStart);
        m_clearUserButton->setEnabled(false);
    }
    replan(false);
    if (m_route.valid)
        m_mapView->fitToRoute();
    updateAdvanceButton();
    updateStatus();
}

void MainWindow::replan(bool firstLoad)
{
    const ScenarioStep &step = ScenarioSteps::at(m_stepIndex);

    m_rankedTargets = TargetRanking::rank(m_scenario.targets, m_travelById);

    // M4 walkback: a target with a route must also leave enough of the EVA
    // time budget to walk back to the safe haven. The haven is the EVA start
    // point (design choice: no real haven coordinate exists in the data), so
    // the return path time equals the outbound path time (symmetric edge
    // cost). The budget comes from the current scenario step (M5).
    QHash<QString, WalkbackResult> walkbackById;
    for (RankedTarget &ranked : m_rankedTargets) {
        ranked.walkbackFailed = false;
        if (!ranked.reachable)
            continue;
        const WalkbackResult check = Walkback::evaluate(
            m_state.elapsedMin, ranked.travelMinutes,
            ranked.target.minutesOnTarget,
            m_oneWay ? 0.0 : ranked.travelMinutes, m_state.timeBudgetMin);
        walkbackById.insert(ranked.target.id, check);
        ranked.walkbackFailed = !check.feasible;
    }

    m_primaryTargetId.clear();
    m_route = Route();
    // S4 (scripted power event): science is disabled, no target is offered.
    if (!step.scienceDisabled) {
        for (const RankedTarget &ranked : m_rankedTargets) {
            if (ranked.reachable && !ranked.walkbackFailed) {
                m_primaryTargetId = ranked.target.id;
                m_route = m_routeById.value(m_primaryTargetId);
                break;
            }
        }
    }

    QStringList planNotes;
    // Greedy multi-target plan (PLAN.md 5.5 steps 3-4) from the EVA start.
    // Its first step is the primary target above. S4 disables science, so the
    // plan is empty there.
    if (step.scienceDisabled) {
        m_plan = MissionPlan();
        m_plan.elapsedAfterMin = m_state.elapsedMin;
    } else if (m_freeMode) {
        m_plan = buildOrderedPlan(&planNotes);
    } else {
        m_plan = MissionPlan::build(
            m_scenario.targets, m_travelById,
            [this](const QString &a, const QString &b) {
                return m_legByPair.value(pairKey(a, b), -1.0);
            },
            m_state.elapsedMin, m_state.timeBudgetMin, m_oneWay);
    }
    // Free route: the first planned stop is the one to go to next.
    if (m_freeMode && !step.scienceDisabled) {
        m_primaryTargetId = m_plan.steps.isEmpty()
                                ? QString()
                                : m_plan.steps.first().targetId;
        m_route = m_primaryTargetId.isEmpty()
                      ? Route()
                      : m_routeById.value(m_primaryTargetId);
    }

    // Draw the whole planned traverse: primary leg first, then the
    // target-to-target legs in plan order.
    QVector<Route> planRoutes;
    if (!m_plan.steps.isEmpty()) {
        const Route firstLeg =
            m_routeById.value(m_plan.steps.first().targetId);
        if (firstLeg.valid)
            planRoutes.append(firstLeg);
        QString previousId = m_plan.steps.first().targetId;
        for (int i = 1; i < m_plan.steps.size(); ++i) {
            const QString nextId = m_plan.steps.at(i).targetId;
            const Route leg =
                m_legRouteByPair.value(pairKey(previousId, nextId));
            if (leg.valid)
                planRoutes.append(leg);
            previousId = nextId;
        }
    }
    if (m_plan.steps.isEmpty())
        m_mapView->clearPlanRoutes();
    else
        m_mapView->setPlanRoutes(planRoutes);

    if (firstLoad) {
        m_mapView->setCrewStart(m_scenario.crewStart);
        const QImage hazardImage =
            m_hazard.hazardImage(QSize(m_hazard.width(), m_hazard.height()));
        m_mapView->setHazardOverlay(hazardImage, m_hazard.extentMeters());
    }
    m_mapView->setTargets(m_rankedTargets, m_primaryTargetId);
    m_mapView->setRoute(m_route);

    // Return leg: the optimal path from the last stop back to the haven. The
    // terrain cost is symmetric, so it is the best direct path, not a retrace
    // of the outward legs. Not drawn for a one-way trip.
    if (!m_plan.steps.isEmpty() && !m_oneWay) {
        Route back = m_routeById.value(m_plan.steps.last().targetId);
        std::reverse(back.points.begin(), back.points.end());
        m_mapView->setReturnRoute(back);
    } else {
        m_mapView->clearReturnRoute();
    }

    m_sidePanel->setScienceGoal(QString::fromUtf8(kScienceGoal));
    m_sidePanel->setRankedTargets(m_rankedTargets);

    RankedTarget primary;
    for (const RankedTarget &ranked : m_rankedTargets) {
        if (ranked.target.id == m_primaryTargetId) {
            primary = ranked;
            break;
        }
    }
    m_sidePanel->setPrimaryTarget(primary, m_route);

    // setPrimaryTarget writes "NO SAFE ROUTE" for any invalid route. Two cases
    // have no route for a reason other than terrain, so the header must say
    // the real reason instead of implying unsafe terrain.
    if (step.scienceDisabled) {
        m_sidePanel->setRouteSummaryText(QStringLiteral(
            "SCIENCE DISABLED: no science route (scripted power event)"));
    } else if (m_primaryTargetId.isEmpty()) {
        bool anyReachable = false;
        for (const RankedTarget &ranked : m_rankedTargets) {
            if (ranked.reachable)
                anyReachable = true;
        }
        if (anyReachable) {
            m_sidePanel->setRouteSummaryText(QStringLiteral(
                "NO TARGET WITHIN BUDGET: routes exist but none passes "
                "the walkback check"));
        }
    }

    if (step.scienceDisabled) {
        m_sidePanel->setWalkbackText(QStringLiteral(
            "SCIENCE DISABLED: return to the haven (EVA start). Crew "
            "position is not simulated."));
        m_sidePanel->setConsumablesText(QStringLiteral(
            "TIME: no science plan. O2 / POWER / WATER: not modeled (the "
            "power drop is a scripted scenario event). ") +
            radiationText(-1.0));
    } else if (!m_primaryTargetId.isEmpty() && m_route.valid &&
               walkbackById.contains(m_primaryTargetId)) {
        const WalkbackResult check = walkbackById.value(m_primaryTargetId);
        m_sidePanel->setWalkbackText(
            QStringLiteral("WALKBACK WITHIN BUDGET: return ~%1 min (x%2) + "
                           "%3 min reserve, margin %4 min. Haven = EVA "
                           "start [design choice].")
                .arg(check.returnMin, 0, 'f', 0)
                .arg(Walkback::kReturnFactor, 0, 'f', 1)
                .arg(Walkback::kReserveMin, 0, 'f', 0)
                .arg(check.marginMin, 0, 'f', 0));
        m_sidePanel->setConsumablesText(
            QStringLiteral("PLAN: %1. TIME: %2 of %3 min EVA budget used "
                           "after the plan, walkback margin %4 min [budget "
                           "is a design choice]. O2 / POWER / WATER: not "
                           "modeled (no endurance data).")
                .arg(m_plan.steps.isEmpty() ? m_primaryTargetId
                                            : m_plan.summary())
                .arg(m_plan.steps.isEmpty() ? check.elapsedAfterMin
                                            : m_plan.elapsedAfterMin,
                     0, 'f', 0)
                .arg(m_state.timeBudgetMin, 0, 'f', 0)
                .arg(m_plan.steps.isEmpty() ? check.marginMin
                                            : m_plan.marginMin,
                     0, 'f', 0)
            + QLatin1Char(' ')
            // EVA length for the dose = time after the last stop + walk back.
            + radiationText(m_plan.steps.isEmpty()
                                ? check.elapsedAfterMin + check.returnMin
                                : m_plan.elapsedAfterMin +
                                      m_plan.steps.last().returnMin));
    } else {
        m_sidePanel->setWalkbackText(
            QStringLiteral("Walkback: no target passes the check."));
        m_sidePanel->setConsumablesText(
            QStringLiteral("TIME: no plan. O2 / POWER / WATER: not modeled "
                           "(no endurance data)."));
    }

    // Trip summary for round trip / one way (free route and one-way only, so
    // the scenario wording stays as before).
    if ((m_freeMode || m_oneWay) && !m_plan.steps.isEmpty() &&
        !step.scienceDisabled) {
        const PlanStep &last = m_plan.steps.last();
        if (m_oneWay) {
            m_sidePanel->setWalkbackText(
                QStringLiteral(
                    "ONE WAY: %1 min travel + work + %2 min reserve = %3 of "
                    "%4 min EVA limit, margin %5 min. No return leg is "
                    "planned; the crew must be recovered at the end.")
                    .arg(m_plan.elapsedAfterMin, 0, 'f', 0)
                    .arg(Walkback::kReserveMin, 0, 'f', 0)
                    .arg(m_plan.elapsedAfterMin + Walkback::kReserveMin, 0,
                         'f', 0)
                    .arg(m_state.timeBudgetMin, 0, 'f', 0)
                    .arg(m_plan.marginMin, 0, 'f', 0));
        } else {
            m_sidePanel->setWalkbackText(
                QStringLiteral(
                    "ROUND TRIP: %1 min out + work, %2 min return (best path, "
                    "x%3) + %4 min reserve = %5 of %6 min EVA limit, margin "
                    "%7 min.")
                    .arg(m_plan.elapsedAfterMin, 0, 'f', 0)
                    .arg(last.returnMin, 0, 'f', 0)
                    .arg(Walkback::kReturnFactor, 0, 'f', 1)
                    .arg(Walkback::kReserveMin, 0, 'f', 0)
                    .arg(m_plan.elapsedAfterMin + last.returnMin +
                             Walkback::kReserveMin, 0, 'f', 0)
                    .arg(m_state.timeBudgetMin, 0, 'f', 0)
                    .arg(m_plan.marginMin, 0, 'f', 0));
        }
    }

    // Scenario context line: state what this step changed, derived from the
    // plan that was actually recomputed above.
    QString contextDetail;
    const QString planSummary = m_plan.summary();
    const QString currentBudget =
        QString::number(qRound(m_state.timeBudgetMin));
    if (step.id == QLatin1String("S0")) {
        contextDetail =
            QStringLiteral("Full plan: %1 \u00b7 %2 min EVA budget.")
                .arg(planSummary, currentBudget);
    } else if (step.id == QLatin1String("S1")) {
        contextDetail =
            QStringLiteral("Dust rising. Plan unchanged: %1 \u00b7 %2 min "
                           "budget.")
                .arg(planSummary, currentBudget);
    } else if (step.id == QLatin1String("S2")) {
        contextDetail =
            QStringLiteral("Storm: EVA budget %1 \u2192 %2 min \u00b7 plan "
                           "now %3.")
                .arg(qRound(m_evaLimitMin))
                .arg(currentBudget, planSummary);
    } else if (step.id == QLatin1String("S3")) {
        contextDetail =
            QStringLiteral("Comms offline +%1 min \u00b7 plan unchanged: "
                           "%2.")
                .arg(m_state.commsOfflineMin)
                .arg(planSummary);
    } else if (step.id == QLatin1String("S4")) {
        contextDetail = QStringLiteral(
            "Power low: science disabled \u00b7 return to the EVA start "
            "(haven).");
    }
    if (m_freeMode) {
        contextDetail = QStringLiteral(
            "Free route from your own points. Scenario S0-S4 is paused; "
            "switch Free route off to return to it.");
        m_sidePanel->setScenarioContext(QStringLiteral("FREE"),
                                        QStringLiteral("FREE ROUTE"),
                                        contextDetail, 0,
                                        ScenarioSteps::count());
    } else {
        m_sidePanel->setScenarioContext(step.id, step.title, contextDetail,
                                        m_stepIndex, ScenarioSteps::count());
    }
    if (m_freeMode && !m_freeStartSet) {
        m_sidePanel->setRouteSummaryText(QStringLiteral(
            "FREE ROUTE: double-click the map to set the START point"));
    } else if (m_freeMode && m_scenario.targets.isEmpty()) {
        m_sidePanel->setRouteSummaryText(QStringLiteral(
            "FREE ROUTE: double-click the map to add a target"));
    }

    QStringList warnings;
    if (!m_route.warning.isEmpty())
        warnings.append(m_route.warning);
    for (const RankedTarget &ranked : m_rankedTargets) {
        if (!ranked.reachable) {
            warnings.append(QStringLiteral("%1 %2 has no safe route.")
                                .arg(ranked.target.id, ranked.target.name));
        } else if (ranked.walkbackFailed) {
            const WalkbackResult check =
                walkbackById.value(ranked.target.id);
            warnings.append(
                QStringLiteral("%1 %2: walkback fails by %3 min (%4 min "
                               "budget).")
                    .arg(ranked.target.id, ranked.target.name)
                    .arg(qRound(-check.marginMin))
                    .arg(qRound(m_state.timeBudgetMin)));
        }
    }
    const HazardMap::Stats stats = m_hazard.stats();
    if (stats.cellCount > 0 && stats.unknownCount > 0) {
        const int unknownPercent =
            qRound(100.0 * static_cast<double>(stats.unknownCount) /
                   static_cast<double>(stats.cellCount));
        warnings.append(
            QStringLiteral("%1% of hazard grid has no data \u2014 treated "
                           "as no-go.")
                .arg(unknownPercent));
    }
    // Per-target standoff notes (a clicked point that is not traversable).
    for (const ScienceTarget &t : m_scenario.targets) {
        if (t.id == m_primaryTargetId)
            continue;  // already added with the primary route above
        const Route r = m_routeById.value(t.id);
        if (r.valid && !r.warning.isEmpty())
            warnings.append(QStringLiteral("%1: %2").arg(t.id, r.warning));
    }
    warnings.append(planNotes);
    if (m_freeMode && !m_freeStartSet)
        warnings.append(QStringLiteral("No START yet: double-click the map."));
    else if (m_freeMode && m_scenario.targets.isEmpty())
        warnings.append(QStringLiteral(
            "No targets yet: double-click the map to add one."));
    if (m_oneWay) {
        warnings.append(QStringLiteral(
            "ONE WAY: no return leg is reserved. Use only when recovery at "
            "the end point is arranged."));
    }
    if (m_evaLimitMin > Walkback::kTimeBudgetMin + 0.5) {
        warnings.append(
            QStringLiteral("EVA limit %1 h is above the 6 h planning "
                           "assumption.")
                .arg(m_evaLimitMin / 60.0, 0, 'f', 1));
    }
    if (!m_plan.steps.isEmpty() && m_plan.marginMin < Walkback::kReserveMin) {
        warnings.append(
            QStringLiteral("Tight plan: only %1 min margin left.")
                .arg(qRound(m_plan.marginMin)));
    }
    if (!m_notice.isEmpty())
        warnings.prepend(QStringLiteral("NOTICE: %1").arg(m_notice));
    m_lastWarnings = warnings;
    m_sidePanel->setWarnings(warnings);

    m_scenarioLoaded = m_hazard.isValid() && !m_primaryTargetId.isEmpty() &&
                       m_route.valid;
    refreshPointList();
}

void MainWindow::addUserTarget(double eastingM, double northingM)
{
    if (!m_missionReady)
        return;
    m_notice.clear();
    if (!m_hazard.contains(eastingM, northingM)) {
        notify(tr("That point is outside the mapped area (no data is never "
                  "treated as safe). Target not added."));
        return;
    }
    for (const ScienceTarget &t : m_scenario.targets) {
        if (QLineF(t.position(), QPointF(eastingM, northingM)).length() <
            kMinPointSpacingM) {
            notify(tr("A point already exists here (%1). Target not added.")
                       .arg(t.id));
            return;
        }
    }
    int userCount = 0;
    for (const ScienceTarget &t : m_scenario.targets) {
        if (t.id.startsWith(QLatin1Char('U')))
            ++userCount;
    }
    if (userCount >= kMaxUserTargets) {
        notify(tr("Target limit reached (%1). Delete a point or use \"Clear "
                  "my targets\".")
                   .arg(kMaxUserTargets));
        return;
    }
    if (m_hazard.isNoGoAt(eastingM, northingM)) {
        notify(tr("That point is on no-go or unknown terrain. The route can "
                  "only end at the nearest safe cell within %1 m.")
                   .arg(TraversePlanner::kMaxSnapM, 0, 'f', 0));
    }

    ScienceTarget target;
    target.id = QStringLiteral("U%1").arg(++m_userTargetSerial);
    target.name = QStringLiteral("User waypoint %1").arg(m_userTargetSerial);
    target.eastingM = eastingM;
    target.northingM = northingM;
    target.scienceValue = kUserTargetValue;
    target.minutesOnTarget = kUserTargetMinutes;
    target.phenomenon = QStringLiteral("User-selected point");
    target.why = QStringLiteral(
        "Chosen by the crew on the map. No science assessment is claimed; "
        "value and work time are design choices.");
    target.source = QStringLiteral("User input (map double-click)");

    // Same planner as the scenario targets: an unreachable point yields an
    // invalid route and is flagged, never treated as safe.
    const Route planned = TraversePlanner::planToTarget(
        m_hazard, m_scenario.crewStart, target);
    m_routeById.insert(target.id, planned);
    m_travelById.insert(target.id, planned.valid ? planned.timeMin : -1.0);
    for (const ScienceTarget &other : m_scenario.targets) {
        const Route leg =
            TraversePlanner::planToTarget(m_hazard, other.position(), target);
        m_legByPair.insert(pairKey(other.id, target.id),
                           leg.valid ? leg.timeMin : -1.0);
        m_legRouteByPair.insert(pairKey(other.id, target.id), leg);
    }
    m_scenario.targets.append(target);

    if (!planned.valid) {
        notify(tr("%1: no safe route from the START to this point. It is "
                  "kept in the list but not planned.")
                   .arg(target.id));
    }
    replan(false);
    if (m_route.valid)
        m_mapView->fitToRoute();
    m_clearUserButton->setEnabled(true);
    updateStatus();
}

void MainWindow::clearUserTargets()
{
    if (!m_missionReady)
        return;
    auto isUser = [](const QString &id) {
        return id.startsWith(QLatin1Char('U'));
    };
    for (int i = m_scenario.targets.size() - 1; i >= 0; --i) {
        if (isUser(m_scenario.targets.at(i).id)) {
            m_routeById.remove(m_scenario.targets.at(i).id);
            m_travelById.remove(m_scenario.targets.at(i).id);
            m_scenario.targets.removeAt(i);
        }
    }
    // Stale pair entries stay in m_leg*ByPair; nothing reads them once the
    // targets are gone, and a re-added Un overwrites them.
    m_userTargetSerial = 0;
    if (m_freeMode) {
        // Free route restarts from scratch: start and targets are both reset.
        m_freeStartSet = false;
        m_routeById.clear();
        m_travelById.clear();
        m_mapView->clearCrewStart();
    } else {
        m_clearUserButton->setEnabled(false);
    }
    replan(false);
    updateStatus();
}

void MainWindow::buildRouteDock()
{
    auto *dock = new QDockWidget(tr("Route"), this);
    dock->setObjectName(QStringLiteral("routeDock"));
    dock->setFeatures(QDockWidget::DockWidgetMovable);
    auto *box = new QWidget(dock);
    auto *layout = new QVBoxLayout(box);

    auto *hint = new QLabel(
        tr("Double-click the map to add a point.\n"
           "Free route: the first click is the START."),
        box);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto *form = new QFormLayout;
    m_tripCombo = new QComboBox(box);
    m_tripCombo->setObjectName(QStringLiteral("tripCombo"));
    m_tripCombo->addItem(tr("Round trip (return to START)"));
    m_tripCombo->addItem(tr("One way (no return)"));
    m_tripCombo->setMinimumHeight(32);
    form->addRow(tr("Trip"), m_tripCombo);

    m_limitSpin = new QDoubleSpinBox(box);
    m_limitSpin->setObjectName(QStringLiteral("evaLimitSpin"));
    m_limitSpin->setRange(0.5, 12.0);
    m_limitSpin->setDecimals(2);
    m_limitSpin->setSingleStep(0.25);
    m_limitSpin->setSuffix(tr(" h"));
    m_limitSpin->setValue(m_evaLimitMin / 60.0);
    m_limitSpin->setMinimumHeight(32);
    m_limitSpin->setToolTip(
        tr("Maximum time outside, including the way back and the %1 min "
           "reserve. 6 h is a planning assumption; edit it if yours differs.")
            .arg(Walkback::kReserveMin, 0, 'f', 0));
    form->addRow(tr("EVA limit"), m_limitSpin);
    layout->addLayout(form);

    layout->addWidget(new QLabel(tr("Points: START, then targets in order"), box));
    m_pointList = new QListWidget(box);
    m_pointList->setObjectName(QStringLiteral("pointList"));
    m_pointList->setMinimumHeight(140);
    layout->addWidget(m_pointList, 1);

    auto *row = new QHBoxLayout;
    const auto makeButton = [&](const QString &text, const QString &name) {
        auto *b = new QPushButton(text, box);
        b->setObjectName(name);
        b->setMinimumHeight(36);
        row->addWidget(b);
        return b;
    };
    auto *del = makeButton(tr("Delete"), QStringLiteral("deletePointButton"));
    auto *up = makeButton(tr("Up"), QStringLiteral("pointUpButton"));
    auto *down = makeButton(tr("Down"), QStringLiteral("pointDownButton"));
    layout->addLayout(row);

    dock->setWidget(box);
    addDockWidget(Qt::LeftDockWidgetArea, dock);

    connect(m_tripCombo, &QComboBox::currentIndexChanged, this,
            [this](int index) { setOneWay(index == 1); });
    connect(m_limitSpin, &QDoubleSpinBox::valueChanged, this,
            &MainWindow::setEvaLimitHours);
    connect(del, &QPushButton::clicked, this,
            &MainWindow::deleteSelectedPoint);
    connect(up, &QPushButton::clicked, this, [this]() { moveSelectedPoint(-1); });
    connect(down, &QPushButton::clicked, this,
            [this]() { moveSelectedPoint(1); });
    auto *deleteKey = new QShortcut(QKeySequence::Delete, m_pointList);
    deleteKey->setContext(Qt::WidgetShortcut);
    connect(deleteKey, &QShortcut::activated, this,
            &MainWindow::deleteSelectedPoint);
}

QListWidget *MainWindow::pointList() const
{
    return m_pointList;
}

double MainWindow::effectiveBudget(const ScenarioStep &step) const
{
    // Nominal steps use the crew's limit; storm steps carry a smaller budget.
    if (step.timeBudgetMin >= Walkback::kTimeBudgetMin)
        return m_evaLimitMin;
    return std::min(step.timeBudgetMin, m_evaLimitMin);
}

void MainWindow::notify(const QString &message)
{
    if (!m_notice.isEmpty())
        m_notice += QLatin1Char(' ');
    m_notice += message;
    QStringList warnings = m_lastWarnings;
    for (int i = warnings.size() - 1; i >= 0; --i) {
        if (warnings.at(i).startsWith(QLatin1String("NOTICE: ")))
            warnings.removeAt(i);
    }
    warnings.prepend(QStringLiteral("NOTICE: %1").arg(m_notice));
    m_sidePanel->setWarnings(warnings);
    updateStatus();
}

void MainWindow::setOneWay(bool oneWay)
{
    if (m_tripCombo) {
        const QSignalBlocker blocker(m_tripCombo);
        m_tripCombo->setCurrentIndex(oneWay ? 1 : 0);
    }
    if (m_oneWay == oneWay)
        return;
    m_oneWay = oneWay;
    m_notice.clear();
    if (m_missionReady) {
        replan(false);
        updateStatus();
    }
}

bool MainWindow::oneWay() const
{
    return m_oneWay;
}

void MainWindow::setEvaLimitHours(double hours)
{
    if (!std::isfinite(hours) || hours < 0.5) {
        notify(tr("EVA limit must be at least 0.5 h. Value ignored."));
        return;
    }
    m_evaLimitMin = hours * 60.0;
    if (m_limitSpin) {
        const QSignalBlocker blocker(m_limitSpin);
        m_limitSpin->setValue(hours);
    }
    m_notice.clear();
    if (m_missionReady) {
        m_state.timeBudgetMin =
            effectiveBudget(ScenarioSteps::at(m_stepIndex));
        replan(false);
        updateStatus();
    }
}

double MainWindow::evaLimitMin() const
{
    return m_evaLimitMin;
}

MissionPlan MainWindow::buildOrderedPlan(QStringList *notes) const
{
    MissionPlan plan;
    plan.elapsedAfterMin = m_state.elapsedMin;
    double elapsed = m_state.elapsedMin;
    QString prev;  // empty = START
    const int count = m_scenario.targets.size();
    for (int i = 0; i < count; ++i) {
        const ScienceTarget &t = m_scenario.targets.at(i);
        const double fromStart = m_travelById.value(t.id, -1.0);
        const double leg =
            prev.isEmpty() ? fromStart
                           : m_legByPair.value(pairKey(prev, t.id), -1.0);
        QString reason;
        if (leg < 0.0) {
            reason = tr("%1 has no safe route from %2.")
                         .arg(t.id, prev.isEmpty() ? tr("the START") : prev);
        } else if (!m_oneWay && fromStart < 0.0) {
            reason = tr("%1 has no safe way back to the START.").arg(t.id);
        }
        WalkbackResult check;
        if (reason.isEmpty()) {
            check = Walkback::evaluate(elapsed, leg, t.minutesOnTarget,
                                       m_oneWay ? 0.0 : fromStart,
                                       m_state.timeBudgetMin);
            if (!check.feasible) {
                reason = tr("%1 exceeds the %2 min EVA limit by %3 min%4.")
                             .arg(t.id)
                             .arg(qRound(m_state.timeBudgetMin))
                             .arg(qRound(-check.marginMin))
                             .arg(m_oneWay ? QString()
                                           : tr(" (including the way back)"));
            }
        }
        if (!reason.isEmpty()) {
            const int skipped = count - i;
            if (notes) {
                notes->append(tr("Route stops before %1: %2").arg(t.id, reason));
                if (skipped > 1) {
                    notes->append(tr("%1 later point(s) are not planned.")
                                      .arg(skipped - 1));
                }
            }
            break;
        }
        PlanStep step;
        step.targetId = t.id;
        step.legMin = leg;
        step.minutesOnTarget = t.minutesOnTarget;
        step.elapsedAfterMin = check.elapsedAfterMin;
        step.returnMin = check.returnMin;
        step.marginMin = check.marginMin;
        plan.steps.append(step);
        elapsed = check.elapsedAfterMin;
        plan.elapsedAfterMin = elapsed;
        plan.marginMin = check.marginMin;
        prev = t.id;
    }
    return plan;
}

void MainWindow::refreshPointList()
{
    if (!m_pointList)
        return;
    const int keep = m_pointList->currentRow();
    const QSignalBlocker blocker(m_pointList);
    m_pointList->clear();
    if (!(m_freeMode && !m_freeStartSet)) {
        m_pointList->addItem(
            tr("START  E %1 · N %2")
                .arg(m_scenario.crewStart.x(), 0, 'f', 0)
                .arg(m_scenario.crewStart.y(), 0, 'f', 0));
        const QStringList planned = m_plan.ids();
        for (const ScienceTarget &t : m_scenario.targets) {
            QString state;
            if (m_travelById.value(t.id, -1.0) < 0.0)
                state = tr("NO ROUTE");
            else if (planned.contains(t.id))
                state = tr("stop %1").arg(planned.indexOf(t.id) + 1);
            else
                state = tr("not planned");
            m_pointList->addItem(
                QStringLiteral("%1 %2 · %3").arg(t.id, t.name, state));
        }
    }
    // Only restore an explicit selection: nothing is pre-selected, so a stray
    // Delete press cannot remove the START.
    if (keep >= 0 && m_pointList->count() > 0)
        m_pointList->setCurrentRow(std::clamp(keep, 0, m_pointList->count() - 1));
}

void MainWindow::refreshAfterEdit()
{
    if (m_freeMode && !m_freeStartSet) {
        m_routeById.clear();
        m_travelById.clear();
        m_legByPair.clear();
        m_legRouteByPair.clear();
        m_mapView->clearCrewStart();
    } else {
        m_mapView->setCrewStart(m_scenario.crewStart);
        rebuildPlanning();
    }
    bool anyUser = false;
    for (const ScienceTarget &t : m_scenario.targets) {
        if (t.id.startsWith(QLatin1Char('U')))
            anyUser = true;
    }
    m_clearUserButton->setEnabled(m_freeMode || anyUser);
    replan(false);
    if (m_route.valid)
        m_mapView->fitToRoute();
    updateStatus();
}

void MainWindow::deleteSelectedPoint()
{
    if (!m_missionReady)
        return;
    m_notice.clear();
    const int row = m_pointList->currentRow();
    if (row < 0) {
        notify(tr("Select a point in the list first."));
        return;
    }
    if (!m_freeMode) {
        if (row == 0) {
            notify(tr("The scenario START cannot be removed. Switch on Free "
                      "route to choose your own."));
            return;
        }
        if (!m_scenario.targets.at(row - 1).id.startsWith(QLatin1Char('U'))) {
            notify(tr("Scenario targets stay in the scenario. Switch on Free "
                      "route to build your own route."));
            return;
        }
        m_scenario.targets.removeAt(row - 1);
    } else if (row == 0) {
        if (m_scenario.targets.isEmpty()) {
            m_freeStartSet = false;
            notify(tr("START removed. Double-click the map to set a new one."));
        } else {
            // The next point takes over as the START (no error).
            const ScienceTarget next = m_scenario.targets.first();
            m_scenario.crewStart = next.position();
            m_scenario.targets.removeFirst();
            notify(tr("START removed. %1 is now the START.").arg(next.id));
        }
    } else {
        m_scenario.targets.removeAt(row - 1);
    }
    refreshAfterEdit();
}

void MainWindow::moveSelectedPoint(int delta)
{
    if (!m_missionReady)
        return;
    m_notice.clear();
    if (!m_freeMode) {
        notify(tr("In the scenario the order is planned automatically. "
                  "Switch on Free route to set your own order."));
        return;
    }
    const int row = m_pointList->currentRow();
    const int idx = row - 1;
    const int target = idx + delta;
    if (row < 1) {
        notify(tr("Select a target to move. The START cannot be moved; "
                  "delete it to promote the next point."));
        return;
    }
    if (target < 0 || target >= m_scenario.targets.size())
        return;  // already at the end of the list
    m_scenario.targets.swapItemsAt(idx, target);
    refreshAfterEdit();
    m_pointList->setCurrentRow(row + delta);
}

void MainWindow::advanceScenario()
{
    if (!m_missionReady)
        return;

    m_stepIndex = (m_stepIndex + 1) % ScenarioSteps::count();
    const ScenarioStep &step = ScenarioSteps::at(m_stepIndex);
    m_state.tau = step.tau;
    m_state.timeBudgetMin = effectiveBudget(step);
    m_state.commsClosed = step.commsClosed;
    m_state.commsOfflineMin = step.commsOfflineMin;

    replan(false);
    if (m_route.valid)
        m_mapView->fitToRoute();
    updateAdvanceButton();
    updateStatus();
}

void MainWindow::updateAdvanceButton()
{
    if (!m_advanceButton)
        return;
    m_advanceButton->setEnabled(m_missionReady && !m_freeMode);
    const bool atLast = m_stepIndex >= ScenarioSteps::count() - 1;
    m_advanceButton->setText(
        atLast ? tr("Restart scenario")
               : tr("Advance scenario (%1)").arg(
                     ScenarioSteps::at(m_stepIndex).id));
}

const DatasetRegistry &MainWindow::registry() const
{
    return m_registry;
}

bool MainWindow::layersLoaded() const
{
    return m_registry.layers().size() == 4;
}

const QString &MainWindow::loadError() const
{
    return m_loadError;
}

LayerLegend *MainWindow::legend() const
{
    return m_legend;
}

bool MainWindow::scenarioLoaded() const
{
    return m_scenarioLoaded;
}

const MissionScenario &MainWindow::scenario() const
{
    return m_scenario;
}

const HazardMap &MainWindow::hazardMap() const
{
    return m_hazard;
}

const QVector<RankedTarget> &MainWindow::rankedTargets() const
{
    return m_rankedTargets;
}

const Route &MainWindow::route() const
{
    return m_route;
}

QString MainWindow::primaryTargetId() const
{
    return m_primaryTargetId;
}

SidePanel *MainWindow::sidePanel() const
{
    return m_sidePanel;
}

int MainWindow::scenarioStepIndex() const
{
    return m_stepIndex;
}

const ScenarioStep &MainWindow::scenarioStep() const
{
    return ScenarioSteps::at(m_stepIndex);
}

const MissionState &MainWindow::missionState() const
{
    return m_state;
}

const MissionPlan &MainWindow::missionPlan() const
{
    return m_plan;
}

bool MainWindow::scienceDisabled() const
{
    return m_missionReady && ScenarioSteps::at(m_stepIndex).scienceDisabled;
}

const SampleCollector *MainWindow::sampleCollector() const
{
    return m_collector.get();
}

int MainWindow::sampleCount() const
{
    return m_collector ? m_collector->log().sampleCount() : 0;
}

QString MainWindow::sampleLogPath() const
{
    return m_collector ? m_collector->logPath() : QString();
}

void MainWindow::openSampleLog()
{
    const RasterLayer *dem = m_registry.layer(QStringLiteral("hirise_dem"));
    const QString path = SampleLog::defaultPath();
    m_collector = std::make_unique<SampleCollector>(dem, path);

    QString error;
    if (!m_collector->load(&error)) {
        m_sampleError = error;
        qCritical().noquote() << "Cetus: sample log load failed:" << error;
        m_sidePanel->setSampleLogError(error);
        m_sidePanel->setCollectTarget(QString());
        if (m_collectToolbarButton)
            m_collectToolbarButton->setEnabled(false);
        return;
    }

    m_sampleError.clear();
    m_sidePanel->setSampleLogError(QString());
    m_sidePanel->setCollectTarget(m_primaryTargetId);

    // The toolbar button is enabled only when there is a target to log at;
    // the tooltip names that target and never claims a physical sample.
    if (m_collectToolbarButton) {
        const bool canCollect = !m_primaryTargetId.isEmpty();
        m_collectToolbarButton->setEnabled(canCollect);
        if (canCollect) {
            m_collectToolbarButton->setToolTip(
                tr("Collect a field note at %1.").arg(m_primaryTargetId));
        }
    }
}

void MainWindow::collectSample()
{
    if (!m_collector || !m_collector->isReady()) {
        m_sampleError = QStringLiteral(
            "sample log is not ready; cannot append a field note.");
        qCritical().noquote() << "Cetus: sample collection failed:"
                              << m_sampleError;
        m_sidePanel->setSampleLogError(m_sampleError);
        updateStatus();
        return;
    }

    const ScienceTarget *target = nullptr;
    for (const RankedTarget &ranked : m_rankedTargets) {
        if (ranked.target.id == m_primaryTargetId) {
            target = &ranked.target;
            break;
        }
    }
    if (!target) {
        m_sampleError = QStringLiteral(
            "no primary target is available; cannot append a field note.");
        qCritical().noquote() << "Cetus: sample collection failed:"
                              << m_sampleError;
        m_sidePanel->setSampleLogError(m_sampleError);
        updateStatus();
        return;
    }

    // [demo] M4 owns the live mission state; the M6 demo logs on sol 3.
    constexpr int kDemoSol = 3;

    SampleRecord record;
    QString error;
    if (!m_collector->collect(*target, kDemoSol,
                              QDateTime::currentDateTimeUtc(), &record,
                              &error)) {
        m_sampleError = error;
        qCritical().noquote() << "Cetus: sample collection failed:" << error;
        m_sidePanel->setSampleLogError(error);
        updateStatus();
        return;
    }

    m_sampleError.clear();
    m_sidePanel->setSampleLogError(QString());
    // The visible note carries the geotag and orientation so the crew can
    // check where the field note was taken and how it was levelled.
    const QString summary =
        QStringLiteral("%1 %2\nE %3 m · N %4 m\n"
                       "up (%5, %6, %7) · slope %8°\n%9")
            .arg(record.targetId, record.targetName)
            .arg(record.eastingM, 0, 'f', 2)
            .arg(record.northingM, 0, 'f', 2)
            .arg(record.upEast, 0, 'f', 2)
            .arg(record.upNorth, 0, 'f', 2)
            .arg(record.upUp, 0, 'f', 2)
            .arg(record.slopeDeg, 0, 'f', 1)
            .arg(record.phenomenon);
    m_sidePanel->setLatestSample(record.id, summary,
                                 m_collector->log().sampleCount());
    m_sidePanel->revealLatestSample();
    updateStatus();
}

void MainWindow::updateStatus()
{
    if (!m_loadError.isEmpty()) {
        statusBar()->showMessage(
            QStringLiteral("RASTER LOAD FAILED: %1").arg(m_loadError));
        return;
    }
    if (!m_missionError.isEmpty()) {
        statusBar()->showMessage(
            QStringLiteral("MISSION DATA FAILED: %1").arg(m_missionError));
        return;
    }
    if (!m_sampleError.isEmpty()) {
        statusBar()->showMessage(
            QStringLiteral("SAMPLE LOG FAILED: %1").arg(m_sampleError));
        return;
    }

    const int layerCount = static_cast<int>(m_registry.layers().size());
    const int visibleCount = m_mapView->visibleLayerIds().size();
    QString text =
        QStringLiteral("SOL %1 · %2 · TAU %3 · %4 layers · %5 visible")
            .arg(m_state.sol)
            .arg(m_state.clock.toString(QStringLiteral("HH:mm")))
            .arg(m_state.tau, 0, 'f', 1)
            .arg(layerCount)
            .arg(visibleCount);
    text += QStringLiteral(" · RAD %1 mSv/d")
                .arg(Radiation::kDoseRateMsvPerDay, 0, 'f', 2);
    if (visibleCount == 0)
        text += QStringLiteral(" · NO LAYERS VISIBLE");
    if (m_missionReady && !m_freeMode) {
        text += QStringLiteral(" · SCENARIO %1")
                    .arg(ScenarioSteps::at(m_stepIndex).id);
    }
    if (m_freeMode) {
        text += !m_freeStartSet ? QStringLiteral(" · FREE ROUTE: SET START")
                                : QStringLiteral(" · FREE ROUTE");
    }
    if (m_state.commsClosed) {
        text += QStringLiteral(" · COMMS OFFLINE +%1 MIN")
                    .arg(m_state.commsOfflineMin);
    }

    if (scienceDisabled()) {
        text += QStringLiteral(" · SCIENCE DISABLED · RETURN TO HAVEN");
    } else if (m_scenarioLoaded && m_route.valid) {
        QString name;
        for (const RankedTarget &ranked : m_rankedTargets) {
            if (ranked.target.id == m_primaryTargetId) {
                name = ranked.target.name;
                break;
            }
        }
        text += QStringLiteral(" · NEXT %1 %2 %3 m")
                    .arg(m_primaryTargetId, name)
                    .arg(qRound(m_route.distanceM));
    } else if (!m_scenario.targets.isEmpty()) {
        // Scenario data loaded but no target can be recommended. Say so
        // rather than implying the plan succeeded.
        bool anyRoute = false;
        for (const RankedTarget &ranked : m_rankedTargets) {
            if (ranked.reachable)
                anyRoute = true;
        }
        text += anyRoute ? QStringLiteral(" · NO TARGET WITHIN BUDGET")
                         : QStringLiteral(" · NO SAFE ROUTE");
    }
    if (m_collector && m_collector->log().sampleCount() > 0) {
        text += QStringLiteral(" · %1 LOGGED")
                    .arg(m_collector->log().records().constLast().id);
    }
    if (!m_notice.isEmpty())
        text = QStringLiteral("WARNING: %1  |  %2").arg(m_notice, text);
    statusBar()->showMessage(text);
}
