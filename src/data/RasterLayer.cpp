#include "data/RasterLayer.h"

#include "gis/PlanetaryCrs.h"

#include <cpl_conv.h>
#include <gdal_priv.h>
#include <ogr_spatialref.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace
{

// RAII wrapper so every failure path in open() closes the dataset and the
// destructor releases the handle exactly once.
struct GdalDatasetCloser
{
    void operator()(GDALDataset *ds) const
    {
        if (ds)
            GDALClose(ds);
    }
};
using DatasetPtr = std::unique_ptr<GDALDataset, GdalDatasetCloser>;

// --- CRS validation -------------------------------------------------------

// True when the SRS is recognisably an Earth CRS (EPSG:4326/3857 codes or
// a name carrying "WGS 84" / "Earth").
bool looksLikeEarthCrs(const OGRSpatialReference &srs)
{
    const char *auth = srs.GetAuthorityName(nullptr);
    const char *code = srs.GetAuthorityCode(nullptr);
    if (auth && code && std::strcmp(auth, "EPSG") == 0 &&
        (std::strcmp(code, "4326") == 0 || std::strcmp(code, "3857") == 0))
        return true;

    const char *name = srs.GetName();
    if (name && (std::strstr(name, "WGS 84") || std::strstr(name, "Earth")))
        return true;
    return false;
}

// Returns an empty string when the CRS is a valid projected Mars CRS,
// otherwise a message that always mentions "Mars" (and "Earth" when the
// input is Earth-based).
QString validateMarsCrs(const OGRSpatialReference &srs)
{
    const bool geographic = srs.IsGeographic() != FALSE;
    const bool earthNamed = looksLikeEarthCrs(srs);
    const double semiMajor = srs.GetSemiMajor(nullptr);
    const bool marsSized =
        std::fabs(semiMajor - PlanetaryCrs::kMarsRadiusM) < 1.0;

    if (geographic || earthNamed || !marsSized)
    {
        QString reason;
        if (geographic)
            reason = QStringLiteral("it is geographic, not projected");
        else if (earthNamed)
            reason = QStringLiteral("its name/authority identifies an Earth CRS");
        else
            reason = QStringLiteral(
                "its semi-major axis is %1 m, not the Mars mean radius %2 m")
                         .arg(semiMajor, 0, 'g', 12)
                         .arg(PlanetaryCrs::kMarsRadiusM, 0, 'f', 0);
        return QStringLiteral(
                   "raster CRS rejected (%1): expected a projected Mars CRS "
                   "(+proj=eqc, R=%2 m); Earth CRS such as EPSG:4326/3857 "
                   "(WGS 84) are never valid here")
            .arg(reason)
            .arg(PlanetaryCrs::kMarsRadiusM, 0, 'f', 0);
    }

    if (!srs.IsProjected())
        return QStringLiteral(
            "raster CRS is not projected: expected a Mars projected CRS "
            "(R=3396190 m)");

    const double linearUnits = srs.GetLinearUnits();
    if (std::fabs(linearUnits - 1.0) > 1e-3)
        return QStringLiteral(
                   "raster CRS linear units are %1 m, not metres: expected "
                   "the Mars projected CRS in metres")
            .arg(linearUnits, 0, 'g', 12);

    return QString();
}

// --- Display ramps (styling only — not data) ------------------------------

struct ColorStop
{
    double t;
    int r, g, b;
};

constexpr ColorStop kTerrainStops[] = {
    {0.00, 4, 8, 20},       // dark blue/black: low ground
    {0.45, 110, 66, 38},    // brown
    {0.75, 214, 186, 140},  // tan
    {1.00, 255, 255, 255},  // white: high ground
};

constexpr ColorStop kHazardStops[] = {
    {0.00, 0, 170, 0},    // green: low hazard
    {0.50, 255, 220, 0},  // yellow
    {0.75, 255, 130, 0},  // orange
    {1.00, 255, 0, 0},    // red: high hazard
};

constexpr ColorStop kGrayscaleStops[] = {
    {0.0, 0, 0, 0},
    {1.0, 255, 255, 255},
};

QRgb sampleStops(const ColorStop *stops, std::size_t count, double t)
{
    t = std::clamp(t, 0.0, 1.0);
    for (std::size_t i = 1; i < count; ++i)
    {
        if (t <= stops[i].t || i == count - 1)
        {
            const ColorStop &a = stops[i - 1];
            const ColorStop &b = stops[i];
            const double span = b.t - a.t;
            const double f = span > 0.0 ? (t - a.t) / span : 0.0;
            const int r = a.r + static_cast<int>(std::lround((b.r - a.r) * f));
            const int g = a.g + static_cast<int>(std::lround((b.g - a.g) * f));
            const int bch = a.b + static_cast<int>(std::lround((b.b - a.b) * f));
            return qRgba(r, g, bch, 255);
        }
    }
    return qRgba(0, 0, 0, 255);
}

QRgb rampColor(RasterLayer::Ramp ramp, double t)
{
    switch (ramp)
    {
        case RasterLayer::Ramp::Terrain:
            return sampleStops(kTerrainStops,
                               sizeof(kTerrainStops) / sizeof(ColorStop), t);
        case RasterLayer::Ramp::Hazard:
            return sampleStops(kHazardStops,
                               sizeof(kHazardStops) / sizeof(ColorStop), t);
        case RasterLayer::Ramp::Grayscale:
            break;
    }
    return sampleStops(kGrayscaleStops,
                       sizeof(kGrayscaleStops) / sizeof(ColorStop), t);
}

// Normalise a sample into 0..1 over the metadata display range, clamping
// anything outside the range.
double normalizeToDisplay(double value, double displayMin, double displayMax)
{
    const double span = displayMax - displayMin;
    if (span <= 0.0)
        return 0.0;
    return std::clamp((value - displayMin) / span, 0.0, 1.0);
}

// Nodata test with a relative tolerance: DEM nodata is ~-3.4e38 where exact
// equality across float/double conversions is fragile. Nodata 0 (hillshade)
// falls back to exact comparison (a relative test would match everything).
bool isNoData(double value, double noData)
{
    if (noData == 0.0)
        return value == 0.0;
    return std::fabs(value - noData) <= std::fabs(noData) * 1e-5;
}

}  // namespace

// --- PIMPL -----------------------------------------------------------------

struct RasterLayer::Impl
{
    GDALDataset *dataset = nullptr;
    RasterLayer::Metadata metadata;
    RasterLayer::Info info;

    ~Impl()
    {
        if (dataset)
            GDALClose(dataset);
    }
};

RasterLayer::RasterLayer(Metadata metadata)
    : m_impl(std::make_unique<Impl>())
{
    m_impl->metadata = std::move(metadata);
}

RasterLayer::~RasterLayer() = default;

RasterLayer::RasterLayer(RasterLayer &&) noexcept = default;

RasterLayer &RasterLayer::operator=(RasterLayer &&) noexcept = default;

bool RasterLayer::open(const QString &path, QString *errorOut)
{
    auto fail = [errorOut](const QString &message) {
        if (errorOut)
            *errorOut = message;
        return false;
    };

    // Drop any previously open dataset before retrying.
    if (m_impl->dataset)
    {
        GDALClose(m_impl->dataset);
        m_impl->dataset = nullptr;
    }
    m_impl->info = Info{};

    if (path.isEmpty())
        return fail(QStringLiteral(
            "empty raster path: the asset directory is unknown "
            "(set CETUS_ASSET_DIR)"));

    GDALAllRegister();
    DatasetPtr ds(static_cast<GDALDataset *>(
        GDALOpenEx(path.toUtf8().constData(),
                   GDAL_OF_RASTER | GDAL_OF_READONLY, nullptr, nullptr,
                   nullptr)));
    if (!ds)
        return fail(QStringLiteral("GDAL could not open raster '%1'").arg(path));

    if (ds->GetRasterXSize() <= 0 || ds->GetRasterYSize() <= 0 ||
        ds->GetRasterCount() < 1)
        return fail(QStringLiteral("raster '%1' has no raster bands").arg(path));

    // --- CRS: must be a projected Mars CRS, never an Earth one. ---
    const OGRSpatialReference *srs = ds->GetSpatialRef();
    if (!srs)
        return fail(QStringLiteral(
            "raster '%1' has no CRS: a projected Mars CRS "
            "(+proj=eqc, R=3396190 m) is required")
            .arg(path));

    const QString crsError = validateMarsCrs(*srs);
    if (!crsError.isEmpty())
        return fail(QStringLiteral("raster '%1': %2").arg(path, crsError));

    // --- Geotransform: strictly north-up, no silent flipping. ---
    double gt[6] = {0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
    if (ds->GetGeoTransform(gt) != CE_None)
        return fail(QStringLiteral(
            "raster '%1' has no geotransform: a north-up Mars grid "
            "is required")
            .arg(path));
    if (gt[2] != 0.0 || gt[4] != 0.0 || gt[1] <= 0.0 || gt[5] >= 0.0)
        return fail(QStringLiteral(
            "raster '%1' is not north-up (gt = [%2, %3, %4, %5, %6, %7]): "
            "rotated/skewed or flipped geotransforms are rejected, "
            "no silent flip")
            .arg(path)
            .arg(gt[0])
            .arg(gt[1])
            .arg(gt[2])
            .arg(gt[3])
            .arg(gt[4])
            .arg(gt[5]));

    // --- Fill Info. ---
    Info info;
    info.width = ds->GetRasterXSize();
    info.height = ds->GetRasterYSize();
    info.pixelSizeX = gt[1];           // > 0 (checked above)
    info.pixelSizeY = -gt[5];          // positive metres per pixel
    // Projected extent: origin (gt[0]) is the west edge; the rectangle is
    // built from the south edge (gt[3] + gt[5]*height) so that y grows
    // northward and bottom() is the north edge (gt[3]).
    info.extent = QRectF(gt[0], gt[3] + gt[5] * info.height,
                         gt[1] * info.width, -gt[5] * info.height);
    info.bandCount = ds->GetRasterCount();

    GDALRasterBand *band = ds->GetRasterBand(1);
    int hasNoData = 0;
    const double noData = band->GetNoDataValue(&hasNoData);
    info.hasNoData = hasNoData != 0;
    info.noDataValue = info.hasNoData ? noData : 0.0;

    info.semiMajorAxisM = srs->GetSemiMajor(nullptr);
    info.isMars =
        std::fabs(info.semiMajorAxisM - PlanetaryCrs::kMarsRadiusM) < 1.0;

    const char *crsName = srs->GetName();
    info.crsName = crsName && *crsName ? QString::fromUtf8(crsName)
                                      : QStringLiteral("unknown");
    char *proj4 = nullptr;
    if (srs->exportToProj4(&proj4) == OGRERR_NONE && proj4)
        info.crsProj4 = QString::fromUtf8(proj4);
    CPLFree(proj4);

    m_impl->info = info;
    m_impl->dataset = ds.release();
    return true;
}

bool RasterLayer::isValid() const
{
    return m_impl && m_impl->dataset != nullptr;
}

const RasterLayer::Metadata &RasterLayer::metadata() const
{
    return m_impl->metadata;
}

const RasterLayer::Info &RasterLayer::info() const
{
    return m_impl->info;
}

QImage RasterLayer::readImage(const QRectF &extentMeters,
                              const QSize &targetPixels) const
{
    QImage out(targetPixels, QImage::Format_ARGB32);
    if (out.isNull())
        return out;
    out.fill(0x00000000u);  // fully transparent

    if (!m_impl || !m_impl->dataset)
        return out;
    const Info &info = m_impl->info;
    if (info.width <= 0 || info.height <= 0 || info.pixelSizeX <= 0.0 ||
        info.pixelSizeY <= 0.0)
        return out;

    // Requested extent (normalised: x grows east, y grows north).
    const double reqWest = qMin(extentMeters.left(), extentMeters.right());
    const double reqEast = qMax(extentMeters.left(), extentMeters.right());
    const double reqSouth = qMin(extentMeters.top(), extentMeters.bottom());
    const double reqNorth = qMax(extentMeters.top(), extentMeters.bottom());
    if (reqEast <= reqWest || reqNorth <= reqSouth)
        return out;

    // Raster bounds (extent is north-up: bottom() is the north edge).
    const double minX = qMin(info.extent.left(), info.extent.right());
    const double maxX = qMax(info.extent.left(), info.extent.right());
    const double minY = qMin(info.extent.top(), info.extent.bottom());
    const double maxY = qMax(info.extent.top(), info.extent.bottom());

    // Clip the request to the raster; disjoint -> stay transparent.
    const double intWest = qMax(reqWest, minX);
    const double intEast = qMin(reqEast, maxX);
    const double intSouth = qMax(reqSouth, minY);
    const double intNorth = qMin(reqNorth, maxY);
    if (intWest >= intEast || intSouth >= intNorth)
        return out;

    // Integer source window in raster pixels (row 0 = north edge).
    const double psx = info.pixelSizeX;
    const double psy = info.pixelSizeY;
    int sx0 = static_cast<int>(std::floor((intWest - minX) / psx));
    int sx1 = static_cast<int>(std::ceil((intEast - minX) / psx));
    int sy0 = static_cast<int>(std::floor((maxY - intNorth) / psy));
    int sy1 = static_cast<int>(std::ceil((maxY - intSouth) / psy));
    sx0 = std::clamp(sx0, 0, info.width);
    sx1 = std::clamp(sx1, 0, info.width);
    sy0 = std::clamp(sy0, 0, info.height);
    sy1 = std::clamp(sy1, 0, info.height);
    if (sx0 >= sx1 || sy0 >= sy1)
        return out;

    // Destination sub-rect derived from the source window so GDAL's
    // resampling maps the window exactly onto the sub-rect. The request
    // maps linearly onto targetPixels; row 0 of the image is the north
    // edge (no flip).
    const double srcWest = minX + sx0 * psx;
    const double srcEast = minX + sx1 * psx;
    const double srcNorth = maxY - sy0 * psy;
    const double srcSouth = maxY - sy1 * psy;
    const double scaleX =
        static_cast<double>(targetPixels.width()) / (reqEast - reqWest);
    const double scaleY =
        static_cast<double>(targetPixels.height()) / (reqNorth - reqSouth);
    int dx0 = static_cast<int>(std::floor((srcWest - reqWest) * scaleX));
    int dx1 = static_cast<int>(std::ceil((srcEast - reqWest) * scaleX));
    int dy0 = static_cast<int>(std::floor((reqNorth - srcNorth) * scaleY));
    int dy1 = static_cast<int>(std::ceil((reqNorth - srcSouth) * scaleY));
    dx0 = std::clamp(dx0, 0, targetPixels.width());
    dx1 = std::clamp(dx1, 0, targetPixels.width());
    dy0 = std::clamp(dy0, 0, targetPixels.height());
    dy1 = std::clamp(dy1, 0, targetPixels.height());
    if (dx0 >= dx1 || dy0 >= dy1)
        return out;

    const int dstW = dx1 - dx0;
    const int dstH = dy1 - dy0;
    GDALRasterBand *band = m_impl->dataset->GetRasterBand(1);
    const GDALDataType srcType = band->GetRasterDataType();
    const Metadata &meta = m_impl->metadata;

    GDALRasterIOExtraArg args;
    INIT_RASTERIO_EXTRA_ARG(args);
    // Byte grids get averaged, float grids bilinearly resampled.
    args.eResampleAlg =
        (srcType == GDT_Byte) ? GRIORA_Average : GRIORA_Bilinear;

    const int srcW = sx1 - sx0;
    const int srcH = sy1 - sy0;

    if (srcType == GDT_Byte)
    {
        std::vector<GByte> buffer(static_cast<std::size_t>(dstW) * dstH);
        const CPLErr err =
            band->RasterIO(GF_Read, sx0, sy0, srcW, srcH, buffer.data(), dstW,
                           dstH, GDT_Byte, 0, 0, &args);
        if (err != CE_None)
            return out;

        for (int row = 0; row < dstH; ++row)
        {
            QRgb *line = reinterpret_cast<QRgb *>(out.scanLine(dy0 + row));
            for (int col = 0; col < dstW; ++col)
            {
                const GByte value = buffer[static_cast<std::size_t>(row) * dstW + col];
                if (info.hasNoData &&
                    static_cast<double>(value) == info.noDataValue)
                {
                    line[dx0 + col] = 0x00000000u;  // nodata -> transparent
                }
                else if (meta.ramp == Ramp::Grayscale)
                {
                    // Byte optical/hillshade: value -> grey, alpha 255.
                    line[dx0 + col] = qRgba(value, value, value, 255);
                }
                else
                {
                    line[dx0 + col] = rampColor(
                        meta.ramp,
                        normalizeToDisplay(value, meta.displayMin,
                                           meta.displayMax));
                }
            }
        }
        return out;
    }

    // Float (or any non-Byte) raster: read as float32 and stretch through
    // the display range with the layer ramp.
    std::vector<float> buffer(static_cast<std::size_t>(dstW) * dstH);
    const CPLErr err =
        band->RasterIO(GF_Read, sx0, sy0, srcW, srcH, buffer.data(), dstW,
                       dstH, GDT_Float32, 0, 0, &args);
    if (err != CE_None)
        return out;

    for (int row = 0; row < dstH; ++row)
    {
        QRgb *line = reinterpret_cast<QRgb *>(out.scanLine(dy0 + row));
        for (int col = 0; col < dstW; ++col)
        {
            const double value =
                buffer[static_cast<std::size_t>(row) * dstW + col];
            if (info.hasNoData && isNoData(value, info.noDataValue))
            {
                line[dx0 + col] = 0x00000000u;  // nodata -> transparent
                continue;
            }
            line[dx0 + col] = rampColor(
                meta.ramp,
                normalizeToDisplay(value, meta.displayMin, meta.displayMax));
        }
    }
    return out;
}
