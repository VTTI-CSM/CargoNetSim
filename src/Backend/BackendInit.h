#pragma once

#include "Backend/Bootstrap/BackendBootstrapService.h"
#include "Backend/Commons/LoggerInterface.h"

#include <QtCore>

namespace CargoNetSim
{
namespace Backend
{

/**
 * @brief Legacy bootstrap wrapper for existing entry points and tests.
 *
 * New code should prefer BackendBootstrapService directly. This helper
 * remains as a compatibility surface while startup call sites migrate to
 * the explicit bootstrap contract.
 *
 * When @p startController is false, only metatype registration runs. This
 * mode is used by tests that need backend signal/slot types but must not
 * start the full controller stack.
 */
inline void
initializeBackend(const QString   &integrationExePath = "",
                  LoggerInterface *logger             = nullptr,
                  bool             startController    = true)
{
    Q_UNUSED(logger);

    const BackendBootstrapService bootstrap;
    const auto result =
        startController
        ? bootstrap.initializeAndStartController(
              integrationExePath)
        : bootstrap.registerOnly();

    if (!result.succeeded())
    {
        const QString message = result.message.isEmpty()
            ? QStringLiteral("backend bootstrap failed")
            : result.message;
        qFatal("initializeBackend: %s",
               qPrintable(message));
    }
}

} // namespace Backend
} // namespace CargoNetSim
