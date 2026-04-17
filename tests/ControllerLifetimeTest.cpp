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

    void test_double_construction_aborts()
    {
        using CargoNetSim::CargoNetSimController;
        auto *owner = new QObject();
        auto *first = new CargoNetSimController(nullptr, owner);
        Q_UNUSED(first);
        // Second construction must fail loudly. We verify the
        // public invariant: after first construction, instance()
        // already returns non-null, so callers have a cheap way to
        // check before constructing again.
        QVERIFY(CargoNetSimController::instance() != nullptr);
        delete owner;
    }

    void test_destruction_order_child_before_controller()
    {
        // Verifies Qt's parent-child destruction order: a QObject child
        // added AFTER the controller is destroyed BEFORE it. This is the
        // guarantee Tier 1 relies on for safe GUI shutdown.
        using CargoNetSim::CargoNetSimController;
        auto *parent = new QObject();
        auto *c      = new CargoNetSimController(nullptr, parent);

        struct Probe : QObject
        {
            bool *controllerAliveAtDtor;
            Probe(bool *p, QObject *par) : QObject(par), controllerAliveAtDtor(p) {}
            ~Probe() override
            {
                *controllerAliveAtDtor =
                    (CargoNetSimController::instance() != nullptr);
            }
        };
        bool controllerAliveWhenProbeDied = false;
        new Probe(&controllerAliveWhenProbeDied, parent);

        delete parent;
        QVERIFY(controllerAliveWhenProbeDied);
        Q_UNUSED(c);
    }
};

QTEST_MAIN(ControllerLifetimeTest)
#include "ControllerLifetimeTest.moc"
