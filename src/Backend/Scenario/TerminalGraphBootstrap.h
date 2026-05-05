#pragma once

#include <QString>

namespace CargoNetSim
{
class CargoNetSimController;

namespace Backend
{
namespace Scenario
{
class ScenarioDocument;
class ScenarioRegistry;

class TerminalGraphBootstrap
{
public:
    static bool resetAndLoad(
        const ScenarioDocument &document,
        const ScenarioRegistry &registry,
        CargoNetSim::CargoNetSimController &controller,
        QString                *error = nullptr,
        const QString          &context = QString());
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
