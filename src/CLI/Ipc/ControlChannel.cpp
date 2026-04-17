#include "ControlChannel.h"

#include <QEventLoop>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QTimer>

#include "Backend/Commons/LogCategories.h"

namespace CargoNetSim {
namespace Cli {

// ---- Server ----------------------------------------------------------------

ControlChannelServer::ControlChannelServer(QObject *parent)
    : QObject(parent)
{
    connect(&m_server, &QLocalServer::newConnection,
            this,     &ControlChannelServer::onNewConnection);
}

ControlChannelServer::~ControlChannelServer()
{
    close();
}

bool ControlChannelServer::listen(const QString &name)
{
    qCInfo(lcCli) << "ControlChannelServer::listen: name =" << name;
    // Crash-recovery: a previous `cargonetsim-cli run` that died
    // without invoking `~ControlChannelServer()` may have left a
    // socket file on disk. `removeServer` clears it; `listen` then
    // claims the name afresh.
    QLocalServer::removeServer(name);
    const bool ok = m_server.listen(name);
    if (!ok)
        qCWarning(lcCli) << "ControlChannelServer::listen: failed —"
                         << m_server.errorString();
    else
        qCDebug(lcCli) << "ControlChannelServer::listen: listening on"
                       << m_server.fullServerName();
    return ok;
}

void ControlChannelServer::setHandler(Handler handler)
{
    qCDebug(lcCli) << "ControlChannelServer::setHandler: handler installed";
    m_handler = std::move(handler);
}

void ControlChannelServer::close()
{
    qCDebug(lcCli) << "ControlChannelServer::close: shutting down server";
    m_server.close();
}

void ControlChannelServer::onNewConnection()
{
    qCDebug(lcCli) << "ControlChannelServer::onNewConnection: incoming connection";
    while (auto *sock = m_server.nextPendingConnection())
    {
        // Per-connection processing lambda. Bound to `this` so the
        // connection is auto-disconnected if the server is destroyed
        // before the client sends a complete line.
        connect(sock, &QLocalSocket::readyRead, this, [this, sock]()
        {
            if (!sock->canReadLine()) return;
            const QByteArray line = sock->readLine().trimmed();
            const auto       doc  = QJsonDocument::fromJson(line);

            QJsonObject resp;
            if (doc.isObject() && m_handler)
                resp = m_handler(doc.object());
            else
                resp[QStringLiteral("error")] =
                    QStringLiteral("bad request");

            sock->write(
                QJsonDocument(resp).toJson(QJsonDocument::Compact));
            sock->write("\n");
            sock->flush();
            sock->disconnectFromServer();
        });

        connect(sock, &QLocalSocket::disconnected,
                sock, &QLocalSocket::deleteLater);
    }
}

// ---- Client ----------------------------------------------------------------

std::optional<QJsonObject> ControlChannelClient::send(
    const QString &serverName, const QJsonObject &request,
    int timeoutMs)
{
    qCDebug(lcCli) << "ControlChannelClient::send: server =" << serverName
                   << ", timeout =" << timeoutMs << "ms"
                   << ", op =" << request.value(QStringLiteral("op")).toString();
    // Event-driven request/response with two state-aware waits:
    //   1. wait until the socket is `Connected` (or timeout / error)
    //   2. wait until a complete line is buffered (or timeout / error)
    //
    // Each wait uses a local `QEventLoop` so Qt signal delivery
    // happens during the wait. This matters for two reasons:
    //   - Same-process tests: the server runs on the same thread and
    //     can only process its `newConnection` / `readyRead` signals
    //     while the client thread spins events. `QLocalSocket::waitFor*`
    //     wait on the socket descriptor only and do NOT pump the Qt
    //     event loop, so the server is starved.
    //   - Cross-process production: the server runs in another process
    //     with its own event loop. Same code path — the local event
    //     loop on the client side just dispatches its own socket
    //     events; behaviour is identical.

    QLocalSocket sock;

    auto waitFor = [&](auto isReady, int ms) -> bool {
        if (isReady()) return true;
        QEventLoop loop;
        QTimer     deadline;
        deadline.setSingleShot(true);
        const auto pump = [&]() {
            if (isReady()) loop.quit();
        };
        QObject::connect(&sock, &QLocalSocket::stateChanged, &loop, pump);
        QObject::connect(&sock, &QLocalSocket::readyRead,    &loop, pump);
        QObject::connect(&sock, &QLocalSocket::errorOccurred,
                         &loop, [&](QLocalSocket::LocalSocketError) {
                             loop.quit();
                         });
        QObject::connect(&deadline, &QTimer::timeout,
                         &loop,    &QEventLoop::quit);
        deadline.start(ms);
        loop.exec();
        return isReady();
    };

    sock.connectToServer(serverName);
    if (!waitFor([&] { return sock.state() == QLocalSocket::ConnectedState; },
                 timeoutMs))
    {
        qCWarning(lcCli) << "ControlChannelClient::send: connect timeout/failure for"
                         << serverName;
        return std::nullopt;
    }

    sock.write(QJsonDocument(request).toJson(QJsonDocument::Compact));
    sock.write("\n");
    sock.flush();

    if (!waitFor([&] { return sock.canReadLine(); }, timeoutMs))
    {
        qCWarning(lcCli) << "ControlChannelClient::send: read timeout for"
                         << serverName;
        return std::nullopt;
    }

    const QByteArray line = sock.readLine().trimmed();
    const auto       doc  = QJsonDocument::fromJson(line);
    if (!doc.isObject())
    {
        qCWarning(lcCli) << "ControlChannelClient::send: response is not a JSON object for"
                         << serverName;
        return std::nullopt;
    }

    qCDebug(lcCli) << "ControlChannelClient::send: received valid response from"
                   << serverName;
    return doc.object();
}

} // namespace Cli
} // namespace CargoNetSim
