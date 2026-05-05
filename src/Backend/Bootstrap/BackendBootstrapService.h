#pragma once

#include <QString>

namespace CargoNetSim
{
namespace Backend
{

enum class BackendBootstrapStatus
{
    Success,
    ControllerMissing,
    InitializeFailed,
    StartFailed
};

struct BackendBootstrapResult
{
    BackendBootstrapStatus status =
        BackendBootstrapStatus::Success;
    QString message;

    bool succeeded() const
    {
        return status == BackendBootstrapStatus::Success;
    }
};

class BackendBootstrapService
{
public:
    static void registerMetatypesOnce();

    BackendBootstrapResult registerOnly() const;

    BackendBootstrapResult initializeController(
        const QString &integrationExePath = QString()) const;

    BackendBootstrapResult startController() const;

    BackendBootstrapResult initializeAndStartController(
        const QString &integrationExePath = QString()) const;
};

} // namespace Backend
} // namespace CargoNetSim
