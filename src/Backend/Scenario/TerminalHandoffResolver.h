#pragma once

#include <QJsonArray>
#include <QString>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

struct TerminalHandoffResolution
{
    enum class Status
    {
        NoOp,
        Ready,
        Error
    };

    Status    status = Status::NoOp;
    QString   scenarioTerminalId;
    QString   containersJson;
    QString   errorMessage;
    qsizetype containerCount = 0;

    bool isReady() const { return status == Status::Ready; }
    bool isNoOp() const { return status == Status::NoOp; }
    bool isError() const { return status == Status::Error; }
};

TerminalHandoffResolution
resolveTerminalHandoff(const QJsonArray &containers);

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
