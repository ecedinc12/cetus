// Radiation estimate tests. Numbers are hand-computed: 0.64 mSv/day * 360/1440
// = 0.16 mSv for the default 360 min EVA.

#include "analysis/Radiation.h"

#include <QtTest>

#include <cmath>

class TestRadiation : public QObject
{
    Q_OBJECT
private slots:
    void fullDayEqualsRate()
    {
        QVERIFY(std::fabs(Radiation::evaDoseMsv(1440.0) - 0.64) < 1e-9);
    }
    void defaultEvaDose()
    {
        QVERIFY(std::fabs(Radiation::evaDoseMsv(360.0) - 0.16) < 1e-9);
    }
    void invalidDurationIsZero()
    {
        QCOMPARE(Radiation::evaDoseMsv(-5.0), 0.0);
        QCOMPARE(Radiation::evaDoseMsv(0.0), 0.0);
    }
};

QTEST_APPLESS_MAIN(TestRadiation)
#include "test_radiation.moc"
