// M2b layer legend tests: row construction from the registry, provenance
// text, toggle signalling, unknown-id handling and registry clearing. Runs
// offscreen (QT_QPA_PLATFORM=offscreen set by CTest).

#include "data/DatasetRegistry.h"
#include "data/RasterLayer.h"
#include "ui/LayerLegend.h"

#include <QCheckBox>
#include <QFile>
#include <QLabel>
#include <QSignalSpy>
#include <QtTest/QTest>

class TestLayerLegend : public QObject
{
    Q_OBJECT
private slots:
    void registryBuildsRows();
    void toggleEmitsOnce();
    void unknownIdIgnored();
    void emptyRegistryClearsRows();
    void mapKeyExplainsOverlaysAndFootprint();

private:
    static QString orthoPath();
    static bool loadRegistry(DatasetRegistry *registry);
};

QString TestLayerLegend::orthoPath()
{
    const QString dir =
        qEnvironmentVariable("CETUS_ASSET_DIR", QStringLiteral("assets"));
    return dir + QStringLiteral("/layers/gale_hirise_ortho_1m.tif");
}

bool TestLayerLegend::loadRegistry(DatasetRegistry *registry)
{
    QString error;
    if (!registry->loadPrepared(&error))
    {
        qWarning("DatasetRegistry::loadPrepared failed: %s", qPrintable(error));
        return false;
    }
    return true;
}

void TestLayerLegend::registryBuildsRows()
{
    if (!QFile::exists(orthoPath()))
        QSKIP("prepared HiRISE assets are not available");

    DatasetRegistry registry;
    QVERIFY(loadRegistry(&registry));

    LayerLegend legend;
    legend.setRegistry(&registry, {QStringLiteral("hirise_optical")});

    QCOMPARE(legend.layerCount(), 4);
    QCOMPARE(static_cast<int>(registry.layers().size()), 4);

    QCheckBox *optical = legend.checkbox(QStringLiteral("hirise_optical"));
    QVERIFY(optical);
    QVERIFY(optical->isChecked());

    QCheckBox *slope = legend.checkbox(QStringLiteral("hirise_slope"));
    QVERIFY(slope);
    QVERIFY(!slope->isChecked());

    for (const auto &raster : registry.layers())
    {
        const RasterLayer::Metadata &meta = raster->metadata();

        QCheckBox *toggle = legend.checkbox(meta.id);
        QVERIFY2(toggle, qPrintable(meta.id));
        QCOMPARE(toggle->text(), meta.title);
        QCOMPARE(toggle->objectName(),
                 QStringLiteral("layerToggle_") + meta.id);
        QCOMPARE(toggle->toolTip(), meta.product);

        QLabel *detail = legend.detailLabel(meta.id);
        QVERIFY2(detail, qPrintable(meta.id));
        QCOMPARE(detail->objectName(),
                 QStringLiteral("layerDetail_") + meta.id);
        QVERIFY2(detail->text().contains(meta.source), qPrintable(detail->text()));
        QVERIFY2(detail->text().contains(meta.resolutionText),
                 qPrintable(detail->text()));
        QVERIFY2(detail->text().contains(meta.units),
                 qPrintable(detail->text()));
    }
}

void TestLayerLegend::toggleEmitsOnce()
{
    if (!QFile::exists(orthoPath()))
        QSKIP("prepared HiRISE assets are not available");

    DatasetRegistry registry;
    QVERIFY(loadRegistry(&registry));

    LayerLegend legend;
    legend.setRegistry(&registry, {QStringLiteral("hirise_optical")});

    QSignalSpy spy(&legend, &LayerLegend::layerToggled);
    QVERIFY(spy.isValid());

    QCheckBox *slope = legend.checkbox(QStringLiteral("hirise_slope"));
    QVERIFY(slope);
    slope->setChecked(true);

    QCOMPARE(spy.count(), 1);
    const QList<QVariant> args = spy.takeFirst();
    QCOMPARE(args.at(0).toString(), QStringLiteral("hirise_slope"));
    QCOMPARE(args.at(1).toBool(), true);
    QVERIFY(legend.visibleLayerIds().contains(QStringLiteral("hirise_slope")));

    legend.setLayerVisible(QStringLiteral("hirise_slope"), false);
    QVERIFY(!legend.isLayerVisible(QStringLiteral("hirise_slope")));
    QCOMPARE(spy.count(), 0);
}

void TestLayerLegend::unknownIdIgnored()
{
    if (!QFile::exists(orthoPath()))
        QSKIP("prepared HiRISE assets are not available");

    DatasetRegistry registry;
    QVERIFY(loadRegistry(&registry));

    LayerLegend legend;
    legend.setRegistry(&registry, {QStringLiteral("hirise_optical")});
    const int rows = legend.layerCount();

    QSignalSpy spy(&legend, &LayerLegend::layerToggled);
    legend.setLayerVisible(QStringLiteral("not_a_layer"), true);
    legend.setLayerVisible(QStringLiteral("not_a_layer"), false);

    QCOMPARE(legend.layerCount(), rows);
    QCOMPARE(spy.count(), 0);
    QVERIFY(!legend.isLayerVisible(QStringLiteral("not_a_layer")));
    QVERIFY(!legend.checkbox(QStringLiteral("not_a_layer")));
    QVERIFY(!legend.detailLabel(QStringLiteral("not_a_layer")));
}

void TestLayerLegend::emptyRegistryClearsRows()
{
    if (!QFile::exists(orthoPath()))
        QSKIP("prepared HiRISE assets are not available");

    DatasetRegistry registry;
    QVERIFY(loadRegistry(&registry));

    LayerLegend legend;
    legend.setRegistry(&registry, {QStringLiteral("hirise_optical")});
    QCOMPARE(legend.layerCount(), 4);

    legend.setRegistry(nullptr);
    QCOMPARE(legend.layerCount(), 0);
    QVERIFY(legend.visibleLayerIds().isEmpty());
    QVERIFY(!legend.checkbox(QStringLiteral("hirise_optical")));
}

void TestLayerLegend::mapKeyExplainsOverlaysAndFootprint()
{
    // The key is static, so no registry or prepared asset is needed.
    LayerLegend legend;
    const QString text = legend.mapKeyText();
    QVERIFY2(!text.isEmpty(), "map key note is empty");

    for (const QString &term : {QStringLiteral("NO-GO"),
                                QStringLiteral("UNKNOWN"),
                                QStringLiteral("CAUTION"),
                                QStringLiteral("ROUTE"),
                                QStringLiteral("rotated"),
                                QStringLiteral("north-up"),
                                QStringLiteral("no data"),
                                QStringLiteral("20")})
    {
        QVERIFY2(text.contains(term, Qt::CaseInsensitive),
                 qPrintable(term + QStringLiteral(" missing from map key")));
    }

    QLabel *note = legend.findChild<QLabel *>(QStringLiteral("mapKeyNote"));
    QVERIFY(note);
    QCOMPARE(note->text(), text);
}

QTEST_MAIN(TestLayerLegend)
#include "test_layerlegend.moc"
