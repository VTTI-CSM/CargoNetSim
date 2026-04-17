#pragma once

#include <QJsonObject>
#include <QLocalServer>
#include <QObject>
#include <QString>

#include <functional>
#include <optional>

namespace CargoNetSim {
namespace Cli {

/**
 * @brief Newline-delimited JSON request/response over `QLocalSocket`.
 *
 * Plan 5 Task 9. Provides the local-only IPC backbone used by the
 * `cargonetsim-cli` subcommands `status` (Task 15) and `stop`
 * (Task 16) to reach a running `run` (Task 17).
 *
 * Protocol — one line per direction, one transaction per connection:
 *   1. client → server: a JSON object terminated by `\n`
 *   2. server → client: a JSON object terminated by `\n`
 *   3. server closes the socket
 *
 * The protocol does not specify keys — the request/response payload
 * is the responsibility of the per-subcommand handler. This wrapper
 * handles only the connection lifecycle and the line-delimited
 * framing.
 *
 * Server side (typical use from `RunCommand`):
 * @code
 *   ControlChannelServer server;
 *   server.setHandler([&rt](const QJsonObject &req) -> QJsonObject {
 *       const QString op = req.value("op").toString();
 *       if (op == "status") return statusFor(rt);
 *       if (op == "stop")   { rt.stop(); return {{"ok", true}}; }
 *       return {{"error", "unknown op"}};
 *   });
 *   server.listen("cargonetsim-cli-" + QString::number(pid));
 * @endcode
 *
 * Client side (typical use from `StatusCommand` / `StopCommand`):
 * @code
 *   auto reply = ControlChannelClient::send(
 *       "cargonetsim-cli-1234",
 *       QJsonObject{{"op", "status"}},
 *       500);  // timeoutMs
 *   if (reply) { ... }
 * @endcode
 */
class ControlChannelServer : public QObject
{
    Q_OBJECT

public:
    /// Handler signature: takes the incoming request object, returns
    /// the response object. Called once per accepted connection.
    using Handler = std::function<QJsonObject(const QJsonObject &request)>;

    explicit ControlChannelServer(QObject *parent = nullptr);
    ~ControlChannelServer() override;

    /// Begin listening on @p name. Removes any stale socket file
    /// first so a previous crashed `run` does not block the new one.
    /// @return true on success, false if the listen failed.
    bool listen(const QString &name);

    /// Install (or replace) the request handler. Without a handler
    /// the server replies `{"error": "bad request"}` to every
    /// well-formed request.
    void setHandler(Handler handler);

    /// Stop accepting new connections and remove the socket file.
    /// In-flight responses are not cancelled. Called automatically
    /// by the destructor.
    void close();

private slots:
    void onNewConnection();

private:
    QLocalServer m_server;
    Handler      m_handler;
};

/**
 * @brief Stateless one-shot client over the same NDJSON protocol.
 *
 * Each `send()` opens a fresh connection, writes one request line,
 * waits for one response line, and closes — matching the server's
 * connection-per-transaction expectation.
 */
class ControlChannelClient
{
public:
    /**
     * @brief Connect, send, await reply, close.
     *
     * @param serverName  Local-socket name the target server is
     *                    listening on (typically
     *                    `cargonetsim-cli-<pid>`).
     * @param request     JSON object to send. Will be serialized with
     *                    compact formatting and terminated by `\n`.
     * @param timeoutMs   Per-stage timeout: connect, then read.
     *                    Default 500 ms suits local transactions.
     *
     * @return the server's response object on success;
     *         `std::nullopt` on any failure (connect timeout, read
     *         timeout, malformed reply, server unreachable).
     */
    static std::optional<QJsonObject> send(
        const QString     &serverName,
        const QJsonObject &request,
        int                timeoutMs = 500);
};

} // namespace Cli
} // namespace CargoNetSim
