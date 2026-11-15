#pragma once

#include "analysis/HazardMap.h"
#include "analysis/TargetRanking.h"
#include "app/MissionScenario.h"
#include "app/MissionState.h"
#include "app/ScenarioSteps.h"
#include "data/DatasetRegistry.h"
#include "routing/MissionPlan.h"
#include "routing/TraversePlanner.h"

#include <QHash>
#include <QMainWindow>
#include <QString>
#include <QVector>

#include <memory>

class LayerLegend;
class MapView;
class QShowEvent;
class QToolButton;
class SampleCollector;
class SidePanel;
class QLabel;

// Top-level application window: toolbar + central map + side panel + layer
// legend + status bar. Loads the four prepared rasters and the M3 mission
// scenario up front; failures are announced loudly in the status bar (no
// modal dialog) and the app stays usable.
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    const DatasetRegistry &registry() const;
    // True when the registry holds all four prepared layers.
    bool layersLoaded() const;
    // Empty on success; otherwise the DatasetRegistry load error.
    const QString &loadError() const;
    // Non-null after construction; the bottom-dock layer toggles.
    LayerLegend *legend() const;

    // True only when the scenario + hazard grid loaded and a primary target
    // has a valid route. Loaded mission data without a safe plan is false.
    bool scenarioLoaded() const;
    const MissionScenario &scenario() const;
    const HazardMap &hazardMap() const;
    const QVector<RankedTarget> &rankedTargets() const;
    const Route &route() const;
    QString primaryTargetId() const;
    SidePanel *sidePanel() const;

    // M5 scripted scenario (S0..S4). advanceScenario() moves to the next step
    // (S4 wraps back to S0) and recomputes ranking, walkback, route and panel.
    // Does nothing when the mission data did not load.
    void advanceScenario();
    // User target: plan routes to/from the point and replan (double-click).
    void addUserTarget(double eastingM, double northingM);
    // Remove all user-placed targets and replan.
    void clearUserTargets();
    // Free-route mode: ignore the scenario targets and start; the crew picks
    // the START (first double-click) and the targets on the map. Leaving the
    // mode restores the scenario untouched.
    void setFreeMode(bool on);
    bool freeMode() const;
    int scenarioStepIndex() const;
    const ScenarioStep &scenarioStep() const;
    const MissionState &missionState() const;
    // Greedy multi-target plan for the current step (empty in S4).
    const MissionPlan &missionPlan() const;
    // True in step S4: no science target is recommended.
    bool scienceDisabled() const;

    // M6 field-note accessors (null/0 when the sample log could not load).
    const SampleCollector *sampleCollector() const;
    int sampleCount() const;
    QString sampleLogPath() const;

protected:
    // Refit once the docking layout has settled on first show (the bottom
    // legend dock changes the central widget's height after MapView's own
    // initial fit). The route view wins when a valid plan exists.
    void showEvent(QShowEvent *event) override;

private:
    // Load the scenario, build the hazard grid, plan one route per target and
    // present exactly one recommendation. Only called when the rasters loaded.
    void loadMission();
    // Recompute ranking, walkback, primary target, map and panel for the
    // current scenario step. `firstLoad` also sets the crew start and the
    // hazard overlay, which never change between steps.
    void replan(bool firstLoad);
    void updateAdvanceButton();
    // Plan routes start->target and target->target for m_scenario.targets.
    void rebuildPlanning();
    // Double-click on the map: sets the free-route start, else adds a target.
    void handleMapDoubleClick(double eastingM, double northingM);
    // Refresh the status-bar mission/layer summary. Keeps the loud load
    // failure message when the rasters could not be loaded.
    void updateStatus();
    // Open the session sample log against the prepared HiRISE DEM. A failure
    // is loud but leaves the map usable.
    void openSampleLog();
    // Append one field note for the current primary target (M6).
    void collectSample();

    MapView *m_mapView;
    SidePanel *m_sidePanel;
    DatasetRegistry m_registry;
    QString m_loadError;
    LayerLegend *m_legend = nullptr;
    QLabel *m_cursorLabel = nullptr;
    // Always-visible twin of the panel's collect button (the panel button can
    // sit below the fold at the demo window size).
    QToolButton *m_collectToolbarButton = nullptr;
    // One-shot refit once the bottom dock has given the map its final size.
    bool m_didInitialFitAfterLayout = false;

    MissionScenario m_scenario;
    HazardMap m_hazard;
    QVector<RankedTarget> m_rankedTargets;
    Route m_route;
    QString m_primaryTargetId;
    QString m_missionError;
    bool m_scenarioLoaded = false;

    MissionState m_state;
    int m_stepIndex = 0;
    // Routes and travel times are planned once (the terrain does not change
    // between scenario steps); only the budget and the flags change.
    QHash<QString, Route> m_routeById;
    QHash<QString, double> m_travelById;
    // Target-to-target travel minutes, keyed by the sorted id pair; negative
    // means no safe route. Planned once, like the routes above.
    QHash<QString, double> m_legByPair;
    // Actual route geometry for each target pair, for drawing the full plan.
    QHash<QString, Route> m_legRouteByPair;
    MissionPlan m_plan;
    bool m_missionReady = false;
    QToolButton *m_advanceButton = nullptr;
    QToolButton *m_clearUserButton = nullptr;
    int m_userTargetSerial = 0;
    QToolButton *m_freeButton = nullptr;
    bool m_freeMode = false;
    bool m_freeStartSet = false;
    MissionScenario m_savedScenario;  // scenario stashed while in free mode

    std::unique_ptr<SampleCollector> m_collector;
    QString m_sampleError;
};
