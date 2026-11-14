#include "ui/MainWindow.h"

#include "data/RasterLayer.h"
#include "data/SiteConfig.h"
#include "routing/Walkback.h"
#include "app/SampleCollector.h"
#include "app/SampleLog.h"
#include "ui/LayerLegend.h"
#include "ui/MapView.h"
#include "ui/SidePanel.h"

#include <QDateTime>
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

    // One header control for the scripted demo scenario (PLAN.md §6).
    toolBar->addSeparator();
    m_advanceButton = new QToolButton(this);
    m_advanceButton->setObjectName(QStringLiteral("advanceButton"));
    m_advanceButton->setText(tr("Advance scenario"));
    m_advanceButton->setEnabled(false);
    toolBar->addWidget(m_advanceButton);

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
    connect(m_advanceButton, &QToolButton::clicked, this,
            &MainWindow::advanceScenario);
    connect(m_collectToolbarButton, &QToolButton::clicked, this,
            &MainWindow::collectSample);
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
    for (int i = 0; i < m_scenario.targets.size(); ++i) {
        for (int j = i + 1; j < m_scenario.targets.size(); ++j) {
            const ScienceTarget &from = m_scenario.targets.at(i);
            const ScienceTarget &to = m_scenario.targets.at(j);
            const Route leg =
                TraversePlanner::planToTarget(m_hazard, from.position(), to);
            m_legByPair.insert(pairKey(from.id, to.id),
                               leg.valid ? leg.timeMin : -1.0);
        }
    }
    m_missionReady = true;

    replan(true);
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
            ranked.target.minutesOnTarget, ranked.travelMinutes,
            m_state.timeBudgetMin);
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

    // Greedy multi-target plan (PLAN.md 5.5 steps 3-4) from the EVA start.
    // Its first step is the primary target above. S4 disables science, so the
    // plan is empty there.
    if (step.scienceDisabled) {
        m_plan = MissionPlan();
        m_plan.elapsedAfterMin = m_state.elapsedMin;
    } else {
        m_plan = MissionPlan::build(
            m_scenario.targets, m_travelById,
            [this](const QString &a, const QString &b) {
                return m_legByPair.value(pairKey(a, b), -1.0);
            },
            m_state.elapsedMin, m_state.timeBudgetMin);
    }

    if (firstLoad) {
        m_mapView->setCrewStart(m_scenario.crewStart);
        const QImage hazardImage =
            m_hazard.hazardImage(QSize(m_hazard.width(), m_hazard.height()));
        m_mapView->setHazardOverlay(hazardImage, m_hazard.extentMeters());
    }
    m_mapView->setTargets(m_rankedTargets, m_primaryTargetId);
    m_mapView->setRoute(m_route);

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
            "power drop is a scripted scenario event)."));
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
                     0, 'f', 0));
    } else {
        m_sidePanel->setWalkbackText(
            QStringLiteral("Walkback: no target passes the check."));
        m_sidePanel->setConsumablesText(
            QStringLiteral("TIME: no plan. O2 / POWER / WATER: not modeled "
                           "(no endurance data)."));
    }

    QStringList warnings;
    if (!step.banner.isEmpty())
        warnings.append(step.banner);
    if (!m_route.warning.isEmpty())
        warnings.append(m_route.warning);
    for (const RankedTarget &ranked : m_rankedTargets) {
        if (!ranked.reachable) {
            warnings.append(QStringLiteral("%1 %2 has no safe route.")
                                .arg(ranked.target.id, ranked.target.name));
        } else if (ranked.walkbackFailed) {
            warnings.append(QStringLiteral("%1 %2: %3")
                                .arg(ranked.target.id, ranked.target.name,
                                     walkbackById.value(ranked.target.id)
                                         .reason));
        }
    }
    const HazardMap::Stats stats = m_hazard.stats();
    if (stats.cellCount > 0 && stats.unknownCount > 0) {
        const int unknownPercent =
            qRound(100.0 * static_cast<double>(stats.unknownCount) /
                   static_cast<double>(stats.cellCount));
        warnings.append(
            QStringLiteral("%1% of the hazard grid is unknown (no data) and "
                           "treated as no-go.")
                .arg(unknownPercent));
    }
    m_sidePanel->setWarnings(warnings);

    m_scenarioLoaded = m_hazard.isValid() && !m_primaryTargetId.isEmpty() &&
                       m_route.valid;
}

void MainWindow::advanceScenario()
{
    if (!m_missionReady)
        return;

    m_stepIndex = (m_stepIndex + 1) % ScenarioSteps::count();
    const ScenarioStep &step = ScenarioSteps::at(m_stepIndex);
    m_state.tau = step.tau;
    m_state.timeBudgetMin = step.timeBudgetMin;
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
    m_advanceButton->setEnabled(m_missionReady);
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
    if (visibleCount == 0)
        text += QStringLiteral(" · NO LAYERS VISIBLE");
    if (m_missionReady) {
        text += QStringLiteral(" · SCENARIO %1")
                    .arg(ScenarioSteps::at(m_stepIndex).id);
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
    statusBar()->showMessage(text);
}
