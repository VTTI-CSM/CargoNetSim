#pragma once

#include <QString>
#include <QStringList>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

/// Output destinations + formats.
struct OutputSpec
{
    QString     directory        = QStringLiteral("./results");
    QStringList formats          = { QStringLiteral("json") };  ///< "json" | "csv"
    bool        containerTracking = true;
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
