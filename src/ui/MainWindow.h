#pragma once

#include "analysis/HazardMap.h"
#include "analysis/TargetRanking.h"
#include "app/MissionScenario.h"
#include "data/DatasetRegistry.h"
#include "routing/TraversePlanner.h"

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

    std::unique_ptr<SampleCollector> m_collector;
    QString m_sampleError;
};
