#include "ui/MainWindow.h"

#include "data/RasterLayer.h"
#include "data/SiteConfig.h"
#include "ui/LayerLegend.h"
#include "ui/MapView.h"
#include "ui/SidePanel.h"

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
// Static MissionState snapshot (M4 links live consumables to the mission).
constexpr const char *kConsumablesSnapshot =
    "O2 100% \u00b7 POWER 100% \u00b7 WATER 100% \u2014 static snapshot "
    "(M4 links mission state)";
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
    if (layersLoaded())
        loadMission();
    updateStatus();

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
    connect(m_mapView, &MapView::cursorMoved, this,
            [this](double eastingM, double northingM) {
                m_cursorLabel->setText(
                    QStringLiteral("E %1 m · N %2 m")
                        .arg(eastingM, 0, 'f', 0)
                        .arg(northingM, 0, 'f', 0));
            });
}

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

    QHash<QString, Route> routeById;
    QHash<QString, double> travelById;
    routeById.reserve(m_scenario.targets.size());
    travelById.reserve(m_scenario.targets.size());
    for (const ScienceTarget &target : m_scenario.targets) {
        const Route planned =
            TraversePlanner::planToTarget(m_hazard, m_scenario.crewStart,
                                          target);
        routeById.insert(target.id, planned);
        // Negative means unreachable; never fabricate a travel time.
        travelById.insert(target.id,
                          planned.valid ? planned.timeMin : -1.0);
    }

    m_rankedTargets = TargetRanking::rank(m_scenario.targets, travelById);

    m_primaryTargetId.clear();
    m_route = Route();
    for (const RankedTarget &ranked : m_rankedTargets) {
        if (ranked.reachable) {
            m_primaryTargetId = ranked.target.id;
            m_route = routeById.value(m_primaryTargetId);
            break;
        }
    }

    m_mapView->setCrewStart(m_scenario.crewStart);
    m_mapView->setTargets(m_rankedTargets, m_primaryTargetId);
    m_mapView->setRoute(m_route);
    const QImage hazardImage =
        m_hazard.hazardImage(QSize(m_hazard.width(), m_hazard.height()));
    m_mapView->setHazardOverlay(hazardImage, m_hazard.extentMeters());

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
    m_sidePanel->setConsumablesText(QString::fromUtf8(kConsumablesSnapshot));

    QStringList warnings;
    if (!m_route.warning.isEmpty())
        warnings.append(m_route.warning);
    for (const RankedTarget &ranked : m_rankedTargets) {
        if (!ranked.reachable) {
            warnings.append(QStringLiteral("%1 %2 has no safe route.")
                                .arg(ranked.target.id, ranked.target.name));
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

    const int layerCount = static_cast<int>(m_registry.layers().size());
    const int visibleCount = m_mapView->visibleLayerIds().size();
    QString text = QStringLiteral("SOL 3 · 08:40 · TAU 0.6 · %1 layers · %2 visible")
                       .arg(layerCount)
                       .arg(visibleCount);
    if (visibleCount == 0)
        text += QStringLiteral(" · NO LAYERS VISIBLE");

    if (m_scenarioLoaded && m_route.valid) {
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
        // Scenario data loaded but no target has a valid route. Say so rather
        // than implying the plan succeeded.
        text += QStringLiteral(" · NO SAFE ROUTE");
    }
    statusBar()->showMessage(text);
}
