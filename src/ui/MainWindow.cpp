#include "ui/MainWindow.h"

#include "ui/MapView.h"
#include "ui/SidePanel.h"

#include <QComboBox>
#include <QDebug>
#include <QDockWidget>
#include <QLabel>
#include <QStatusBar>
#include <QToolBar>
#include <QToolButton>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_mapView(new MapView(this))
    , m_sidePanel(new SidePanel(this))
{
    setWindowTitle(tr("Cetus — Mars Surface Field Tool"));
    resize(1280, 800);

    setCentralWidget(m_mapView);

    auto *panelDock = new QDockWidget(tr("Field Panel"), this);
    panelDock->setObjectName(QStringLiteral("fieldPanelDock"));
    panelDock->setWidget(m_sidePanel);
    panelDock->setFeatures(QDockWidget::DockWidgetMovable);
    addDockWidget(Qt::RightDockWidgetArea, panelDock);

    // Map toolbar: layer selector + fit / zoom controls.
    QToolBar *toolBar = addToolBar(tr("Map"));
    toolBar->setObjectName(QStringLiteral("mapToolBar"));
    toolBar->setMovable(false);

    m_layerCombo = new QComboBox(this);
    m_layerCombo->setObjectName(QStringLiteral("layerCombo"));
    m_layerCombo->setMinimumWidth(220);
    m_layerCombo->setToolTip(tr("Raster layer"));
    toolBar->addWidget(m_layerCombo);

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
        statusBar()->showMessage(
            QStringLiteral("RASTER LOAD FAILED: %1").arg(error));
        qCritical().noquote() << "Cetus: raster load failed:" << error;
    } else {
        for (const auto &layer : m_registry.layers())
            m_layerCombo->addItem(layer->metadata().title,
                                  layer->metadata().id);
        m_mapView->setRegistry(&m_registry);
        const int idx = m_layerCombo->findData(m_mapView->activeLayerId());
        if (idx >= 0)
            m_layerCombo->setCurrentIndex(idx);
        statusBar()->showMessage(
            QStringLiteral("SOL 3 · 08:40 · TAU 0.6 · 4 layers"));
    }

    connect(m_layerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
                m_mapView->setActiveLayer(
                    m_layerCombo->currentData().toString());
            });
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
