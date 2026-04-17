#include <QCoreApplication>
#include <QTest>

#include "Backend/Controllers/CargoNetSimController.h"

class ControllerLifetimeTest : public QObject
{
    Q_OBJECT
private slots:
    void test_instance_returns_nullptr_before_construction()
    {
        // New API: instance() is a pure lookup. If no controller exists,
        // it must return nullptr rather than creating one.
        QVERIFY(CargoNetSim::CargoNetSimController::instance() == nullptr);
    }

    void test_instance_returns_controller_after_construction()
    {
        using CargoNetSim::CargoNetSimController;
        auto *owner = new QObject();
        auto *c     = new CargoNetSimController(nullptr, owner);
        QCOMPARE(CargoNetSimController::instance(), c);
        delete owner; // deletes c as child
        QVERIFY(CargoNetSimController::instance() == nullptr);
    }

    void test_instance_survives_inner_scope_declared_later()
    {
        // Tier 1 ownership: main() stack-allocates the controller
        // above MainWindow, so the controller outlives any object
        // declared later. This test encodes that guarantee: a probe
        // in an inner scope observes the controller alive during its
        // own destruction.
        using CargoNetSim::CargoNetSimController;
        CargoNetSimController controller(nullptr);

        struct Probe
        {
            bool *aliveAtDtor;
            explicit Probe(bool *p) : aliveAtDtor(p) {}
            ~Probe()
            {
                *aliveAtDtor =
                    (CargoNetSimController::instance() != nullptr);
            }
        };

        bool aliveWhenProbeDied = false;
        {
            Probe p(&aliveWhenProbeDied);
        } // ~Probe runs here, controller still alive in outer scope

        QVERIFY(aliveWhenProbeDied);
    }
};

QTEST_MAIN(ControllerLifetimeTest)
#include "ControllerLifetimeTest.moc"
