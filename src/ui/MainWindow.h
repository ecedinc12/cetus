#pragma once

#include "data/DatasetRegistry.h"

#include <QMainWindow>
#include <QString>

class MapView;
class SidePanel;
class QComboBox;
class QLabel;

// Top-level application window: toolbar + central map + side panel + status
// bar (M1c). Loads the four prepared rasters up front and fails loudly in
// the status bar (no modal dialog) when the assets are missing.
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

    const DatasetRegistry &registry() const;
    // True when the registry holds all four prepared layers.
    bool layersLoaded() const;
    // Empty on success; otherwise the DatasetRegistry load error.
    const QString &loadError() const;

private:
    MapView *m_mapView;
    SidePanel *m_sidePanel;
    DatasetRegistry m_registry;
    QString m_loadError;
    QComboBox *m_layerCombo = nullptr;
    QLabel *m_cursorLabel = nullptr;
};
