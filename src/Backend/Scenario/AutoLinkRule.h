#pragma once

#include "Connection.h"
#include "GlobalLink.h"
#include "NodeLinkage.h"
#include <QList>
#include <QString>
#include <QVariant>
#include <functional>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

class ScenarioDocument;
class ScenarioRegistry;

/// Rule typedefs. Rules are pure functions of the document + registry; they
/// never mutate either. Returning an empty list is always valid.
using LinkageRuleFn =
    std::function<QList<NodeLinkage>(const ScenarioDocument &,
                                     const ScenarioRegistry &,
                                     const QString &region)>;

using ConnectionRuleFn =
    std::function<QList<Connection>(const ScenarioDocument &,
                                    const ScenarioRegistry &,
                                    const QString &region)>;

using GlobalRuleFn =
    std::function<QList<GlobalLink>(const ScenarioDocument &,
                                    const ScenarioRegistry &,
                                    const QVariant &ruleParam)>;

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
