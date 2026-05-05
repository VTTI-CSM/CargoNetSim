// tests/PathDistancePopulatorTest.cpp
#include <QTest>

#include "Backend/Models/Path.h"
#include "Backend/Models/PathSegment.h"
#include "Backend/Scenario/PathDistancePopulator.h"
#include "Backend/Scenario/ScenarioDocument.h"

// The populator reaches into controllers that are normally
// singletons. For a pure unit test, we exercise only the ship
// branch (uses the document's globalPositionOf + config —
// both swappable without touching NetworkController). The rail
// + truck branches are covered via the GUI integration test in
// Task 6 where real networks are loaded.

using namespace CargoNetSim::Backend;
using namespace CargoNetSim::Backend::Scenario;

class PathDistancePopulatorTest : public QObject
{
    Q_OBJECT
private slots:
    void test_ship_segment_populated_from_haversine()
    {
        // TODO: test body requires (a) a ScenarioDocument with two
        // terminals with LatLon positions, (b) a ConfigController
        // returning a map with ship.average_speed. Build the
        // scenario via ScenarioDocument's public API (addRegion,
        // addTerminal with mode=LatLon); build a Path containing
        // one PathSegment with mode=Ship; call populate(); assert
        // seg->estimatedDistance() ≈ haversineMeters(coords).
        //
        // Construction of a real NetworkController without loading
        // scenarios is non-trivial — pass nullptr where the
        // populator accepts it; adapt populator to tolerate null
        // for non-ship branches if that simplifies testability.
        QSKIP("Fill in body per Task 3 Step 3 plan; see comment above");
    }
};

QTEST_MAIN(PathDistancePopulatorTest)
#include "PathDistancePopulatorTest.moc"
