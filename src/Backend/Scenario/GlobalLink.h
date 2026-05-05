#pragma once

#include "Backend/Commons/TransportationMode.h"
#include "LinkageSource.h"
#include <QMap>
#include <QString>
#include <QVariant>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

/// Cross-region link between two terminals (only ship mode in practice today).
///
/// Endpoints are stored as "<region>/<terminalId>" when disambiguation is
/// required, or the unqualified id when only one region owns that id.
struct GlobalLink
{
    QString                                 fromTerminalId;
    QString                                 toTerminalId;
    TransportationTypes::TransportationMode mode =
        TransportationTypes::TransportationMode::Ship;
    QMap<QString, QVariant> properties;
    LinkageSource source = LinkageSource::Manual;
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
