#pragma once

#include <QString>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

/// A single validator finding.
struct ValidationIssue
{
    enum Severity
    {
        Error   = 0,
        Warning = 1,
    };

    Severity severity = Error;
    QString  path;     ///< Dotted path into the document, e.g. "regions[USA].terminals[T1]".
    QString  message;
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
