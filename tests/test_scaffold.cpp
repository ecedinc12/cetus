#include "app/MissionState.h"
#include "gis/PlanetaryCrs.h"

#include <QtTest>

#include <algorithm>
#include <cmath>

// M0 scaffold tests: Mars CRS constants/strings and the plain MissionState
// struct. Science logic gets its own tests in later milestones.
class TestScaffold : public QObject
{
    Q_OBJECT

private slots:
    void marsRadiusConstant();
    void marsEquirectangularIsEpglessProjString();
    void marsEquirectangularFormatsLonZero();
    void eqcToLonLatOrigin();
    void lonLatEqcRoundTrip();
    void anchorInsideProjectedBBox();
    void missionStateIsPlainData();
};

void TestScaffold::marsRadiusConstant()
{
    QCOMPARE(PlanetaryCrs::kMarsRadiusM, 3396190.0);
}

void TestScaffold::marsEquirectangularIsEpglessProjString()
{
    // Gale Crater reference central meridian (PLAN.md §4 example).
    const QString proj = PlanetaryCrs::marsEquirectangular(137.44);

    QVERIFY(proj.startsWith(QLatin1Char('+')));
    QVERIFY(proj.contains(QStringLiteral("+proj=eqc")));
    QVERIFY(proj.contains(QStringLiteral("+lat_ts=0")));
    QVERIFY(proj.contains(QStringLiteral("+lon_0=137.44")));
    QVERIFY(proj.contains(QStringLiteral("+R=3396190")));
    QVERIFY(proj.contains(QStringLiteral("+units=m")));
    QVERIFY(proj.contains(QStringLiteral("+no_defs")));

    // Must be EPSG-less and never Earth-based.
    QVERIFY(!proj.contains(QStringLiteral("EPSG")));
    QVERIFY(!proj.contains(QStringLiteral("WGS84")));
    QVERIFY(!proj.contains(QStringLiteral("utm")));
}

void TestScaffold::marsEquirectangularFormatsLonZero()
{
    QCOMPARE(PlanetaryCrs::marsEquirectangular(0.0),
             QStringLiteral(
                 "+proj=eqc +lat_ts=0 +lon_0=0 +R=3396190 +units=m +no_defs"));
    QCOMPARE(PlanetaryCrs::marsEquirectangular(-74.5),
             QStringLiteral(
                 "+proj=eqc +lat_ts=0 +lon_0=-74.5 +R=3396190 +units=m +no_defs"));
}

void TestScaffold::eqcToLonLatOrigin()
{
    const PlanetaryCrs::LonLat origin =
        PlanetaryCrs::eqcToLonLat(0.0, 0.0, 137.39);
    QVERIFY(std::abs(origin.lonDeg - 137.39) < 1e-9);
    QVERIFY(std::abs(origin.latDeg - 0.0) < 1e-9);
}

void TestScaffold::lonLatEqcRoundTrip()
{
    const QPointF p = PlanetaryCrs::lonLatToEqc(137.381, -4.695, 137.39);
    const PlanetaryCrs::LonLat back =
        PlanetaryCrs::eqcToLonLat(p.x(), p.y(), 137.39);
    QVERIFY(std::abs(back.lonDeg - 137.381) < 1e-9);
    QVERIFY(std::abs(back.latDeg - (-4.695)) < 1e-9);
}

void TestScaffold::anchorInsideProjectedBBox()
{
    // Site anchor must fall inside the bbox once every corner is projected
    // with the site central meridian (137.39).
    constexpr double kCentralMeridian = 137.39;
    const QPointF sw = PlanetaryCrs::lonLatToEqc(137.33518211,   // west
                                                 -4.792202743651,  // south
                                                 kCentralMeridian);
    const QPointF ne = PlanetaryCrs::lonLatToEqc(137.44270087,   // east
                                                 -4.583317928209,  // north
                                                 kCentralMeridian);
    const double minX = std::min(sw.x(), ne.x());
    const double maxX = std::max(sw.x(), ne.x());
    const double minY = std::min(sw.y(), ne.y());
    const double maxY = std::max(sw.y(), ne.y());

    const QPointF anchor =
        PlanetaryCrs::lonLatToEqc(137.381, -4.695, kCentralMeridian);
    QVERIFY(anchor.x() >= minX && anchor.x() <= maxX);
    QVERIFY(anchor.y() >= minY && anchor.y() <= maxY);
}

void TestScaffold::missionStateIsPlainData()
{
    MissionState state;
    state.sol = 12;
    state.tau = 1.1;
    state.o2Pct = 88.0;
    state.powerPct = 42.0;
    state.waterPct = 77.0;

    QCOMPARE(state.sol, 12);
    QCOMPARE(state.tau, 1.1);
    QCOMPARE(state.o2Pct, 88.0);
    QCOMPARE(state.powerPct, 42.0);
    QCOMPARE(state.waterPct, 77.0);
}

QTEST_APPLESS_MAIN(TestScaffold)
#include "test_scaffold.moc"
