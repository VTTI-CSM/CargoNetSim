#pragma once

#include <QString>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

class ScenarioDocument;
struct TerminalPlacement;

struct ResolvedTerminalEndpoint
{
    bool                     found = false;
    QString                  terminalId;
    QString                  region;
    const TerminalPlacement *terminal = nullptr;
};

/// Resolves a terminal endpoint that may be either "terminalId" or
/// "region/terminalId". The returned terminal pointer is owned by the
/// document and remains valid only while the document is not mutated.
ResolvedTerminalEndpoint resolveTerminalEndpoint(
    const ScenarioDocument &document,
    const QString          &endpoint);

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
