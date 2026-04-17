// tests/GUI/TransportationModeConversionTest.cpp
#include <QTest>

#include "GUI/Commons/TransportationModeConversion.h"

using namespace CargoNetSim::GUI;
using Mode = CargoNetSim::Backend::TransportationTypes::TransportationMode;

class TransportationModeConversionTest : public QObject
{
    Q_OBJECT
private slots:
    void test_each_network_type_maps_to_matching_transportation_mode()
    {
        QCOMPARE(transportationModeFromNetworkType(NetworkType::Train), Mode::Train);
        QCOMPARE(transportationModeFromNetworkType(NetworkType::Truck), Mode::Truck);
        QCOMPARE(transportationModeFromNetworkType(NetworkType::Ship),  Mode::Ship);
    }
};

QTEST_MAIN(TransportationModeConversionTest)
#include "TransportationModeConversionTest.moc"
