#include "analysis/HazardMap.h"

#include "data/RasterLayer.h"

#include <QColor>

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{

double minX(const QRectF &r)
{
    return qMin(r.left(), r.right());
}

double maxX(const QRectF &r)
{
    return qMax(r.left(), r.right());
}

double minY(const QRectF &r)
{
    return qMin(r.top(), r.bottom());
}

double maxY(const QRectF &r)
{
    return qMax(r.top(), r.bottom());
}

}  // namespace

bool HazardMap::build(const RasterLayer &slopeLayer, double cellSizeM,
                      QString *errorOut)
{
    auto fail = [errorOut](const QString &message) {
        if (errorOut)
            *errorOut = message;
        return false;
    };

    if (errorOut)
        errorOut->clear();

    if (!slopeLayer.isValid())
        return fail(QStringLiteral(
            "hazard grid: slope layer is not open or has no raster band"));

    if (!std::isfinite(cellSizeM) || cellSizeM <= 0.0)
        return fail(QStringLiteral("hazard grid: cell size %1 m is invalid")
                        .arg(cellSizeM, 0, 'g', 12));

    const RasterLayer::Info &info = slopeLayer.info();
    const QRectF extent = info.extent;
    const double widthM = maxX(extent) - minX(extent);
    const double heightM = maxY(extent) - minY(extent);
    if (!std::isfinite(widthM) || !std::isfinite(heightM) || widthM <= 0.0 ||
        heightM <= 0.0)
        return fail(QStringLiteral(
            "hazard grid: slope layer has an empty or invalid extent"));

    const int width = static_cast<int>(std::ceil(widthM / cellSizeM));
    const int height = static_cast<int>(std::ceil(heightM / cellSizeM));
    if (width <= 0 || height <= 0)
        return fail(QStringLiteral(
            "hazard grid: cell size %1 m produced an empty %2x%3 grid")
                        .arg(cellSizeM, 0, 'g', 12)
                        .arg(width)
                        .arg(height));

    bool readOk = false;
    const QVector<float> values =
        slopeLayer.readValues(extent, width, height, &readOk);
    if (!readOk || values.size() != width * height)
        return fail(QStringLiteral(
            "hazard grid: could not read slope samples for a %1x%2 grid "
            "(got %3 values)")
                        .arg(width)
                        .arg(height)
                        .arg(values.size()));

    QVector<double> slopes(width * height);
    Stats stats;
    stats.cellCount = width * height;

    for (int i = 0; i < values.size(); ++i)
    {
        const double value = values.at(i);
        if (!std::isfinite(value))
        {
            slopes[i] = std::numeric_limits<double>::quiet_NaN();
            ++stats.unknownCount;
            continue;
        }

        slopes[i] = value;
        stats.maxSlopeDeg = std::max(stats.maxSlopeDeg, value);
        if (value > kNoGoSlopeDeg)
            ++stats.noGoCount;
    }

    m_extent = extent;
    m_cellSizeM = cellSizeM;
    m_width = width;
    m_height = height;
    m_slopeDeg = std::move(slopes);
    m_stats = stats;

    const RasterLayer::Metadata &meta = slopeLayer.metadata();
    const QString source = meta.source.isEmpty()
                               ? QStringLiteral("slope")
                               : meta.source + QStringLiteral(" slope");
    const QString resolution = meta.resolutionText.isEmpty()
                                   ? QStringLiteral("unknown resolution")
                                   : meta.resolutionText;
    const QString units =
        meta.units.isEmpty() ? QStringLiteral("degrees") : meta.units;
    m_sourceDescription =
        QStringLiteral("%1 · %2 · %3 · no-go > %4°")
            .arg(source, resolution, units,
                 QString::number(kNoGoSlopeDeg, 'f', 0));

    return true;
}

bool HazardMap::isValid() const
{
    return m_width > 0 && m_height > 0 &&
           m_slopeDeg.size() == m_width * m_height;
}

QRectF HazardMap::extentMeters() const
{
    return m_extent;
}

double HazardMap::cellSizeM() const
{
    return m_cellSizeM;
}

int HazardMap::width() const
{
    return m_width;
}

int HazardMap::height() const
{
    return m_height;
}

double HazardMap::cellWidthM() const
{
    return m_width > 0 ? (maxX(m_extent) - minX(m_extent)) / m_width : 0.0;
}

double HazardMap::cellHeightM() const
{
    return m_height > 0 ? (maxY(m_extent) - minY(m_extent)) / m_height : 0.0;
}

bool HazardMap::inside(int col, int row) const
{
    return col >= 0 && col < m_width && row >= 0 && row < m_height;
}

bool HazardMap::contains(double eastM, double northM) const
{
    if (!isValid())
        return false;
    return eastM >= minX(m_extent) && eastM <= maxX(m_extent) &&
           northM >= minY(m_extent) && northM <= maxY(m_extent);
}

double HazardMap::slopeDeg(int col, int row) const
{
    if (!inside(col, row) || !isValid())
        return std::numeric_limits<double>::quiet_NaN();
    return m_slopeDeg.at(row * m_width + col);
}

bool HazardMap::isNoGo(int col, int row) const
{
    if (!inside(col, row) || !isValid())
        return true;
    const double slope = m_slopeDeg.at(row * m_width + col);
    if (!std::isfinite(slope))
        return true;  // unknown is never safe to traverse
    return slope > kNoGoSlopeDeg;
}

bool HazardMap::isNoGoAt(double eastM, double northM) const
{
    if (!isValid() || !std::isfinite(eastM) || !std::isfinite(northM))
        return true;
    if (!contains(eastM, northM))
        return true;

    const double cw = cellWidthM();
    const double ch = cellHeightM();
    if (cw <= 0.0 || ch <= 0.0)
        return true;

    const int col = std::clamp(
        static_cast<int>(std::floor((eastM - minX(m_extent)) / cw)), 0,
        m_width - 1);
    const int row = std::clamp(
        static_cast<int>(std::floor((maxY(m_extent) - northM) / ch)), 0,
        m_height - 1);
    return isNoGo(col, row);
}

double HazardMap::cost(int col, int row) const
{
    if (isNoGo(col, row))
        return std::numeric_limits<double>::infinity();

    const double slope = slopeDeg(col, row);
    const double normalized = slope / kNoGoSlopeDeg;
    return 1.0 + 2.0 * normalized * normalized;
}

QPointF HazardMap::cellCenter(int col, int row) const
{
    if (!inside(col, row) || !isValid())
        return QPointF();

    const double cw = cellWidthM();
    const double ch = cellHeightM();
    return QPointF(minX(m_extent) + (col + 0.5) * cw,
                   maxY(m_extent) - (row + 0.5) * ch);
}

QImage HazardMap::hazardImage(const QSize &targetPixels) const
{
    if (!isValid() || targetPixels.width() <= 0 ||
        targetPixels.height() <= 0)
        return QImage();

    QImage image(targetPixels, QImage::Format_ARGB32);
    if (image.isNull())
        return image;
    image.fill(0x00000000u);

    for (int y = 0; y < targetPixels.height(); ++y)
    {
        QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        const int row = std::clamp(
            static_cast<int>(static_cast<qint64>(y) * m_height /
                             targetPixels.height()),
            0, m_height - 1);
        for (int x = 0; x < targetPixels.width(); ++x)
        {
            const int col = std::clamp(
                static_cast<int>(static_cast<qint64>(x) * m_width /
                                 targetPixels.width()),
                0, m_width - 1);
            const double slope = m_slopeDeg.at(row * m_width + col);
            if (!std::isfinite(slope) || slope > kNoGoSlopeDeg)
                line[x] = qRgba(220, 30, 20, 170);
            else if (slope >= 10.0)
                line[x] = qRgba(255, 170, 0, 120);
        }
    }
    return image;
}

HazardMap::Stats HazardMap::stats() const
{
    return m_stats;
}

QString HazardMap::sourceDescription() const
{
    return m_sourceDescription;
}
