#pragma once

#include <QList>
#include <QObject>
#include <QString>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

/**
 * @brief Backend-pure server connectivity probe.
 *
 * Replaces the inline loop in GUI/Controllers/HeartbeatController.cpp:129-239.
 * Returns structured data — callers decide how to display it. The GUI wires
 * this to its status-bar QLabels; the CLI wires it to stdout / exit codes.
 *
 * Stateless: no cached last-check timestamps, no monitoring thread. Callers
 * (HeartbeatController's QTimer, CLI one-shot poll) pace it themselves.
 */
class ServerStatusProbe : public QObject
{
    Q_OBJECT
public:
    struct ServerStatus
    {
        QString server;
        bool    clientExists = false;
        bool    connected    = false;
        bool    hasConsumers = false;
        bool    commandAvailable = false;
    };

    explicit ServerStatusProbe(QObject *parent = nullptr);

    /**
     * @brief Polls all four clients once and returns their status.
     *
     * Reads CargoNetSimController::getInstance() on the calling thread.
     * Exceptions are swallowed per-client (mirrors the existing try/catch
     * block in HeartbeatController.cpp:146-238) so a single bad client
     * does not mask the others.
     */
    QList<ServerStatus> pollAll();
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
