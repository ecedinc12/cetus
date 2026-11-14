#include "routing/TraversePlanner.h"

#include "analysis/HazardMap.h"
#include "analysis/TargetRanking.h"

#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <queue>
#include <utility>
#include <vector>

namespace
{

constexpr double kInf = std::numeric_limits<double>::infinity();

struct SnapResult
{
    bool ok = false;
    int col = 0;
    int row = 0;
    double distanceM = 0.0;   // requested point -> cell centre
    double standoffM = 0.0;   // requested point -> cell footprint (0 inside)
};

double minX(const QRectF &r)
{
    return std::min(r.left(), r.right());
}

double maxX(const QRectF &r)
{
    return std::max(r.left(), r.right());
}

double minY(const QRectF &r)
{
    return std::min(r.top(), r.bottom());
}

double maxY(const QRectF &r)
{
    return std::max(r.top(), r.bottom());
}

// Nearest traversable cell to `point`, searched in expanding Chebyshev rings
// around the clamped cell so a far-out point cannot scan the whole grid.
// Unknown cells are no-go (HazardMap::isNoGo), so the result is never a cell
// that is unsafe to enter.
SnapResult snapToSafeCell(const HazardMap &hazard, const QPointF &point)
{
    SnapResult result;
    if (!hazard.isValid())
        return result;

    const QRectF extent = hazard.extentMeters();
    const double cellWidth =
        (maxX(extent) - minX(extent)) / hazard.width();
    const double cellHeight =
        (maxY(extent) - minY(extent)) / hazard.height();
    if (!std::isfinite(cellWidth) || !std::isfinite(cellHeight) ||
        cellWidth <= 0.0 || cellHeight <= 0.0)
        return result;

    const int baseCol = std::clamp(
        static_cast<int>(std::floor((point.x() - minX(extent)) / cellWidth)),
        0, hazard.width() - 1);
    const int baseRow = std::clamp(
        static_cast<int>(std::floor((maxY(extent) - point.y()) / cellHeight)),
        0, hazard.height() - 1);

    const double minStep = std::min(cellWidth, cellHeight);
    const double maxStep = std::max(cellWidth, cellHeight);
    const int maxRadius =
        static_cast<int>(std::ceil(TraversePlanner::kMaxSnapM / minStep)) + 2;
    double bestDistance = kInf;
    int bestIndex = std::numeric_limits<int>::max();

    for (int radius = 0; radius <= maxRadius; ++radius)
    {
        // A cell at Chebyshev radius `radius` is at least
        // radius*minStep - half a cell from the requested point (the point
        // can sit at the edge of its base cell), so once we have a closer
        // candidate the outer rings cannot win.
        if (result.ok &&
            radius * minStep - 0.5 * maxStep > result.distanceM)
            break;

        for (int dRow = -radius; dRow <= radius; ++dRow)
        {
            const int dColAbs = radius - std::abs(dRow);
            const int row = baseRow + dRow;
            if (row < 0 || row >= hazard.height())
                continue;

            for (int side = 0; side < (dColAbs == 0 ? 1 : 2); ++side)
            {
                const int dCol = side == 0 ? -dColAbs : dColAbs;
                const int col = baseCol + dCol;
                if (col < 0 || col >= hazard.width())
                    continue;
                if (hazard.isNoGo(col, row))
                    continue;

                const QPointF center = hazard.cellCenter(col, row);
                const double distance = std::hypot(center.x() - point.x(),
                                                   center.y() - point.y());
                if (!std::isfinite(distance) ||
                    distance > TraversePlanner::kMaxSnapM)
                    continue;

                const int index = row * hazard.width() + col;
                const bool closer = distance < bestDistance - 1e-9;
                const bool tied =
                    std::fabs(distance - bestDistance) <= 1e-9 &&
                    index < bestIndex;
                if (!result.ok || closer || tied)
                {
                    result.ok = true;
                    result.col = col;
                    result.row = row;
                    result.distanceM = distance;
                    bestDistance = distance;
                    bestIndex = index;
                }
            }
        }
    }

    if (result.ok)
    {
        // `standoffM` is the distance from the requested point to the safe
        // cell the route actually ends in. A pin that lies inside that cell
        // is reached, so it cannot be "short" of the route end; only a pin
        // outside the safe cell (e.g. on a no-go slope) has a real standoff.
        const QPointF center =
            hazard.cellCenter(result.col, result.row);
        const double dx =
            std::max(0.0, std::fabs(point.x() - center.x()) - 0.5 * cellWidth);
        const double dy =
            std::max(0.0, std::fabs(point.y() - center.y()) - 0.5 * cellHeight);
        result.standoffM = std::hypot(dx, dy);
    }

    return result;
}

QString formatMeters(double metres)
{
    return QString::number(std::max(0.0, metres), 'f', 0);
}

}  // namespace

Route TraversePlanner::plan(const HazardMap &hazard, const QPointF &start,
                            const QPointF &goal)
{
    Route route;
    if (!hazard.isValid())
    {
        route.warning = QStringLiteral(
            "cannot plan: the hazard grid is not built.");
        return route;
    }

    const double widthM = maxX(hazard.extentMeters()) -
                          minX(hazard.extentMeters());
    const double heightM = maxY(hazard.extentMeters()) -
                           minY(hazard.extentMeters());
    const double cellWidth = widthM / hazard.width();
    const double cellHeight = heightM / hazard.height();
    if (!std::isfinite(cellWidth) || !std::isfinite(cellHeight) ||
        cellWidth <= 0.0 || cellHeight <= 0.0)
    {
        route.warning =
            QStringLiteral("cannot plan: the hazard grid has no cell size.");
        return route;
    }

    const SnapResult startSnap = snapToSafeCell(hazard, start);
    if (!startSnap.ok)
    {
        route.warning =
            QStringLiteral("crew start is on a no-go cell or outside the "
                           "hazard grid; no safe cell within %1 m.")
                .arg(formatMeters(kMaxSnapM));
        return route;
    }

    const SnapResult goalSnap = snapToSafeCell(hazard, goal);
    if (!goalSnap.ok)
    {
        route.warning =
            QStringLiteral("goal is on a no-go cell or outside the hazard "
                           "grid; no safe cell within %1 m.")
                .arg(formatMeters(kMaxSnapM));
        return route;
    }

    const int width = hazard.width();
    const int height = hazard.height();
    const int startIndex = startSnap.row * width + startSnap.col;
    const int goalIndex = goalSnap.row * width + goalSnap.col;
    const int cellCount = width * height;

    const auto heuristic = [&](int index) {
        const int col = index % width;
        const int row = index / width;
        const QPointF here = hazard.cellCenter(col, row);
        const QPointF target = hazard.cellCenter(goalSnap.col, goalSnap.row);
        // Minimum cost multiplier is 1.0, so plain Euclidean metres is an
        // admissible heuristic.
        return std::hypot(here.x() - target.x(), here.y() - target.y());
    };

    std::vector<double> gScore(cellCount, kInf);
    std::vector<int> previous(cellCount, -1);
    std::vector<char> closed(cellCount, 0);
    gScore[startIndex] = 0.0;

    // Min-heap on (f, cell index): deterministic tie-breaking on the lower
    // index keeps repeated plans byte-identical.
    using QueueEntry = std::pair<double, int>;
    std::priority_queue<QueueEntry, std::vector<QueueEntry>,
                        std::greater<QueueEntry>>
        open;
    open.push({heuristic(startIndex), startIndex});

    const int dCols[8] = {1, -1, 0, 0, 1, 1, -1, -1};
    const int dRows[8] = {0, 0, 1, -1, 1, -1, 1, -1};

    bool reached = false;
    while (!open.empty())
    {
        const QueueEntry entry = open.top();
        open.pop();
        const int current = entry.second;
        if (closed[current])
            continue;
        closed[current] = 1;
        if (current == goalIndex)
        {
            reached = true;
            break;
        }

        const int col = current % width;
        const int row = current / width;
        const double currentCost = hazard.cost(col, row);
        if (!std::isfinite(currentCost))
            continue;  // defensive: no-go cells never enter the open set

        for (int k = 0; k < 8; ++k)
        {
            const int dCol = dCols[k];
            const int dRow = dRows[k];
            const int nextCol = col + dCol;
            const int nextRow = row + dRow;
            if (nextCol < 0 || nextCol >= width || nextRow < 0 ||
                nextRow >= height)
                continue;
            if (hazard.isNoGo(nextCol, nextRow))
                continue;

            // No corner cutting: a diagonal may not slip between two no-go
            // orthogonal neighbours.
            if (dCol != 0 && dRow != 0)
            {
                if (hazard.isNoGo(col + dCol, row) ||
                    hazard.isNoGo(col, row + dRow))
                    continue;
            }

            const int nextIndex = nextRow * width + nextCol;
            if (closed[nextIndex])
                continue;

            const double stepM = std::hypot(dCol * cellWidth, dRow * cellHeight);
            const double edgeCost =
                0.5 * (currentCost + hazard.cost(nextCol, nextRow)) * stepM;
            const double tentative = gScore[current] + edgeCost;
            if (tentative + 1e-12 < gScore[nextIndex])
            {
                gScore[nextIndex] = tentative;
                previous[nextIndex] = current;
                open.push({tentative + heuristic(nextIndex), nextIndex});
            }
        }
    }

    if (!reached)
    {
        route.warning = QStringLiteral(
            "no safe traverse exists between the snapped start and goal: the "
            "traversable cells are disconnected.");
        return route;
    }

    std::vector<int> path;
    for (int index = goalIndex; index != -1; index = previous[index])
        path.push_back(index);
    std::reverse(path.begin(), path.end());

    route.points.reserve(static_cast<int>(path.size()));
    double geometric = 0.0;
    double maxSlope = 0.0;
    for (int i = 0; i < static_cast<int>(path.size()); ++i)
    {
        const int col = path[i] % width;
        const int row = path[i] / width;
        const QPointF center = hazard.cellCenter(col, row);
        route.points.append(center);

        const double slope = hazard.slopeDeg(col, row);
        if (std::isfinite(slope))
            maxSlope = std::max(maxSlope, slope);

        if (i > 0)
        {
            const QPointF previousPoint = route.points.at(i - 1);
            geometric += std::hypot(center.x() - previousPoint.x(),
                                    center.y() - previousPoint.y());
        }
    }

    route.distanceM = geometric;
    route.timeMin = gScore[goalIndex] / kSuitSpeedMps / 60.0;
    route.maxSlopeDeg = maxSlope;
    route.standoffM = goalSnap.standoffM;
    route.valid = true;

    if (route.standoffM > 0.0)
    {
        route.warning =
            QStringLiteral("goal is not traversable; route stops %1 m short "
                           "at a safe standoff.")
                .arg(formatMeters(route.standoffM));
    }

    return route;
}

Route TraversePlanner::planToTarget(const HazardMap &hazard,
                                    const QPointF &start,
                                    const ScienceTarget &target)
{
    Route route = plan(hazard, start, target.position());
    if (!route.valid)
        return route;

    if (route.standoffM > 0.0)
    {
        const bool insideGrid = hazard.contains(target.eastingM,
                                                target.northingM);
        const QString reason =
            insideGrid ? QStringLiteral("is on a no-go slope")
                       : QStringLiteral("is outside the hazard grid");
        route.warning =
            QStringLiteral("%1 pin %2; route stops %3 m short at a safe "
                           "standoff.")
                .arg(target.name, reason, formatMeters(route.standoffM));
    }

    return route;
}
