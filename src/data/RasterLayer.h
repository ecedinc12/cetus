#pragma once

// M1a raster layer: provenance (Metadata), GDAL-derived grid facts (Info)
// and pixel rendering (readImage). GDAL lives entirely behind this module —
// the header must not include GDAL headers (AGENTS.md §6), so the dataset
// handle is held in a PIMPL.

#include <QImage>
#include <QRectF>
#include <QSize>
#include <QString>

#include <memory>

class RasterLayer
{
public:
    enum class Kind { Optical, Elevation, Slope, Hillshade };
    enum class Ramp { Grayscale, Terrain, Hazard };

    struct Metadata
    {
        QString id, title, source, product, resolutionText, units, fileName;
        Kind kind = Kind::Optical;
        Ramp ramp = Ramp::Grayscale;
        double displayMin = 0.0, displayMax = 255.0;
    };

    struct Info
    {
        int width = 0, height = 0;
        double pixelSizeX = 0.0, pixelSizeY = 0.0;  // positive metres/pixel
        QRectF extent;  // projected metres, north-up; x grows east,
                        // y grows north (bottom() is the north edge)
        QString crsName, crsProj4;
        double semiMajorAxisM = 0.0;
        bool isMars = false;
        int bandCount = 0;
        bool hasNoData = false;
        double noDataValue = 0.0;
    };

    explicit RasterLayer(Metadata metadata);
    ~RasterLayer();
    RasterLayer(const RasterLayer &) = delete;
    RasterLayer &operator=(const RasterLayer &) = delete;
    RasterLayer(RasterLayer &&) noexcept;
    RasterLayer &operator=(RasterLayer &&) noexcept;

    // Opens the raster read-only and validates it is a north-up Mars grid.
    // On failure returns false and writes a message to *errorOut (which
    // mentions "Mars", and "Earth" when the input is an Earth CRS).
    bool open(const QString &path, QString *errorOut);
    bool isValid() const;
    const Metadata &metadata() const;
    const Info &info() const;

    // Renders the requested projected extent into targetPixels. Row 0 of
    // the image is the north edge (rasters are north-up; no flip). Pixels
    // outside the raster or on nodata are fully transparent.
    QImage readImage(const QRectF &extentMeters,
                     const QSize &targetPixels) const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
