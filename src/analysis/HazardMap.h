#pragma once

// Slope-hazard grid for the traverse planner (PLAN.md §5.3): a regular
// metre-grid derived from the prepared slope layer. Nodata/outside cells are
// unknown and therefore no-go — missing data is never treated as safe
// (AGENTS.md §4).

#include <QImage>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>

class RasterLayer;

class HazardMap
{
public:
    // [tunable] PLAN.md §5.3: slope above this is a hard no-go.
    static constexpr double kNoGoSlopeDeg = 20.0;
    // [tunable] route grid resolution.
    static constexpr double kCellSizeM = 10.0;

    struct Stats
    {
        int cellCount = 0;
        int noGoCount = 0;
        int unknownCount = 0;
        double maxSlopeDeg = 0.0;
    };

    bool build(const RasterLayer &slopeLayer, double cellSizeM,
               QString *errorOut = nullptr);
    bool isValid() const;
    QRectF extentMeters() const;    // same convention as RasterLayer::Info::extent
    double cellSizeM() const;
    int width() const;
    int height() const;
    bool contains(double eastM, double northM) const;
    double slopeDeg(int col, int row) const;      // NaN for unknown/invalid
    bool isNoGo(int col, int row) const;          // unknown/outside => true
    bool isNoGoAt(double eastM, double northM) const;
    double cost(int col, int row) const;          // infinity for no-go
    QPointF cellCenter(int col, int row) const;
    QImage hazardImage(const QSize &targetPixels) const;
    Stats stats() const;
    QString sourceDescription() const;

private:
    bool inside(int col, int row) const;
    double cellWidthM() const;
    double cellHeightM() const;

    QRectF m_extent;
    double m_cellSizeM = 0.0;
    int m_width = 0;
    int m_height = 0;
    QVector<double> m_slopeDeg;  // row-major, row 0 = north; NaN = unknown
    Stats m_stats;
    QString m_sourceDescription;
};
