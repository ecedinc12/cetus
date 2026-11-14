#include "data/DatasetRegistry.h"

#include "data/SiteConfig.h"

bool DatasetRegistry::loadPrepared(QString *errorOut)
{
    const auto descriptors = SiteConfig::preparedLayerMetadata();

    // Build the full set locally so a failure leaves the registry empty
    // instead of holding a half-loaded mix.
    std::vector<std::unique_ptr<RasterLayer>> loaded;
    loaded.reserve(descriptors.size());

    for (const RasterLayer::Metadata &meta : descriptors)
    {
        const QString path = SiteConfig::layerPath(meta.fileName);
        if (path.isEmpty())
        {
            if (errorOut)
                *errorOut = QStringLiteral(
                    "layer '%1': asset directory unknown, set CETUS_ASSET_DIR")
                    .arg(meta.id);
            return false;
        }

        auto raster = std::make_unique<RasterLayer>(meta);
        QString openError;
        if (!raster->open(path, &openError))
        {
            if (errorOut)
                *errorOut = QStringLiteral("layer '%1' ('%2'): %3")
                                .arg(meta.id, path, openError);
            return false;
        }
        loaded.push_back(std::move(raster));
    }

    m_layers = std::move(loaded);
    return true;
}

const std::vector<std::unique_ptr<RasterLayer>> &DatasetRegistry::layers() const
{
    return m_layers;
}

const RasterLayer *DatasetRegistry::layer(const QString &id) const
{
    for (const auto &raster : m_layers)
    {
        if (raster->metadata().id == id)
            return raster.get();
    }
    return nullptr;
}

const RasterLayer *DatasetRegistry::defaultLayer() const
{
    return layer(QString::fromLatin1(SiteConfig::kDefaultLayerId));
}

QStringList DatasetRegistry::ids() const
{
    QStringList out;
    out.reserve(static_cast<int>(m_layers.size()));
    for (const auto &raster : m_layers)
        out.append(raster->metadata().id);
    return out;
}
