#include "LogMessageHandler.h"

#include "LoggerInterface.h"

#include <QByteArray>
#include <QMessageLogContext>
#include <QString>

#include <cstring>

namespace CargoNetSim
{
namespace Backend
{

namespace
{
LoggerInterface  *s_guiLogger       = nullptr;
QtMessageHandler  s_previousHandler = nullptr;

const char k_prefix[] = "cargonetsim.";
constexpr int k_prefixLen =
    sizeof(k_prefix) - 1; // exclude null terminator

void cargoNetSimLogHandler(
    QtMsgType                type,
    const QMessageLogContext &ctx,
    const QString            &msg)
{
    // Bridge info/warning/critical from cargonetsim.*
    // categories to the GUI logger for user-visible display.
    if (s_guiLogger && type != QtDebugMsg)
    {
        const char *cat = ctx.category;
        if (cat
            && std::strncmp(cat, k_prefix, k_prefixLen) == 0)
        {
            const int  clientIdx = categoryToClientType(cat);
            const bool isError   = (type == QtCriticalMsg
                                  || type == QtFatalMsg);
            if (isError)
                s_guiLogger->logError(msg, clientIdx);
            else
                s_guiLogger->log(msg, clientIdx);
        }
    }

    // Always delegate to the previous handler (Qt default)
    // for stderr output with standard formatting.
    if (s_previousHandler)
        s_previousHandler(type, ctx, msg);
}
} // anonymous namespace

int categoryToClientType(const char *category)
{
    if (!category)
        return 4;

    // Exact matches for the four modal client categories.
    if (std::strcmp(category, "cargonetsim.client.ship") == 0)
        return 0; // ShipClient
    if (std::strcmp(category, "cargonetsim.client.train") == 0
        || std::strcmp(category, "cargonetsim.rail") == 0)
        return 1; // TrainClient
    if (std::strcmp(category, "cargonetsim.client.truck")
        == 0)
        return 2; // TruckClient
    if (std::strcmp(category,
                    "cargonetsim.client.terminal")
        == 0)
        return 3; // TerminalClient

    // Everything else (rabbitmq, client base, controllers,
    // config, model, scenario, gui.*, init, threading)
    // routes to the General/CargoNetSim tab.
    return 4;
}

void installCargoNetSimLogHandler(
    LoggerInterface *guiLogger)
{
    s_guiLogger       = guiLogger;
    s_previousHandler =
        qInstallMessageHandler(cargoNetSimLogHandler);
}

} // namespace Backend
} // namespace CargoNetSim
