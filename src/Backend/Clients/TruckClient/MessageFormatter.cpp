/**
 * @file MessageFormatter.cpp
 * @brief Implements standard message formats for simulation
 * @author Ahmed Aredah
 * @date 2025-03-22
 */

#include "MessageFormatter.h"
#include "Backend/Commons/LogCategories.h"
#include <QJsonDocument>

namespace CargoNetSim
{
namespace Backend
{
namespace TruckClient
{

QString MessageFormatter::formatMessage(
    int msgId, bool acknowledgement,
    MessageType messageType, MessageCode messageCode,
    const QString &content)
{
    if (msgId < 0)
    {
        qCWarning(lcClientTruck)
            << "MessageFormatter::formatMessage:"
            << "negative msgId=" << msgId;
    }
    if (content.isEmpty())
    {
        qCWarning(lcClientTruck)
            << "MessageFormatter::formatMessage:"
            << "empty content for msgId=" << msgId;
    }

    qCDebug(lcClientTruck)
        << "MessageFormatter::formatMessage:"
        << "msgId=" << msgId
        << "type=" << static_cast<int>(messageType)
        << "code=" << static_cast<int>(messageCode);

    // Standard format:
    // id/ack/type/code/00/00/00/00/content/-1
    return QString("%1/%2/%3/%4/00/00/00/00/%5/-1")
        .arg(msgId)
        .arg(acknowledgement ? "1" : "0")
        .arg(static_cast<int>(messageType))
        .arg(static_cast<int>(messageCode))
        .arg(content);
}

QString MessageFormatter::formatSyncRequest(
    int msgId, double simTime, double simHorizon)
{
    if (msgId < 0 || simTime < 0 || simHorizon < 0)
    {
        qCWarning(lcClientTruck)
            << "MessageFormatter::formatSyncRequest:"
            << "invalid input: msgId=" << msgId
            << "simTime=" << simTime
            << "simHorizon=" << simHorizon;
    }

    qCDebug(lcClientTruck)
        << "MessageFormatter::formatSyncRequest:"
        << "msgId=" << msgId
        << "simTime=" << simTime
        << "simHorizon=" << simHorizon;

    QString content =
        QString("%1/%2").arg(simTime).arg(simHorizon);

    return formatMessage(msgId, false, MessageType::SYNC,
                         MessageCode::SYNC_REQ, content);
}

QString MessageFormatter::formatSyncGo(int    msgId,
                                       double currentTime,
                                       double nextTime)
{
    if (msgId < 0 || currentTime < 0 || nextTime < 0)
    {
        qCWarning(lcClientTruck)
            << "MessageFormatter::formatSyncGo:"
            << "invalid input: msgId=" << msgId
            << "currentTime=" << currentTime
            << "nextTime=" << nextTime;
    }

    qCDebug(lcClientTruck)
        << "MessageFormatter::formatSyncGo:"
        << "msgId=" << msgId
        << "currentTime=" << currentTime
        << "nextTime=" << nextTime;

    QString content =
        QString("%1/%2")
            .arg(static_cast<int>(currentTime))
            .arg(static_cast<int>(nextTime));

    return formatMessage(msgId, true, MessageType::SYNC,
                         MessageCode::SYNC_GO, content);
}

QString MessageFormatter::formatSyncEnd(int    msgId,
                                        double simTime)
{
    if (msgId < 0 || simTime < 0)
    {
        qCWarning(lcClientTruck)
            << "MessageFormatter::formatSyncEnd:"
            << "invalid input: msgId=" << msgId
            << "simTime=" << simTime;
    }

    qCDebug(lcClientTruck)
        << "MessageFormatter::formatSyncEnd:"
        << "msgId=" << msgId
        << "simTime=" << simTime;

    QString content =
        QString("%1").arg(static_cast<int>(simTime));

    return formatMessage(msgId, true, MessageType::SYNC,
                         MessageCode::SYNC_END, content);
}

QString MessageFormatter::formatAddTrip(
    int msgId, int tripId, int originId, int destinationId,
    double startTime, const QList<int> &linkIds)
{
    if (msgId < 0 || tripId < 0 || startTime < 0)
    {
        qCWarning(lcClientTruck)
            << "MessageFormatter::formatAddTrip:"
            << "invalid input: msgId=" << msgId
            << "tripId=" << tripId
            << "startTime=" << startTime;
    }
    if (linkIds.isEmpty())
    {
        qCWarning(lcClientTruck)
            << "MessageFormatter::formatAddTrip:"
            << "empty linkIds for tripId=" << tripId;
    }

    qCDebug(lcClientTruck)
        << "MessageFormatter::formatAddTrip:"
        << "msgId=" << msgId
        << "tripId=" << tripId
        << "origin=" << originId
        << "dest=" << destinationId
        << "linkCount=" << linkIds.size();

    // Format content with link count and links
    QString content = QString("%1/%2/%3/%4/%5")
                          .arg(tripId)
                          .arg(originId)
                          .arg(destinationId)
                          .arg(static_cast<int>(startTime))
                          .arg(linkIds.size());

    // Add each link ID to the content
    for (int linkId : linkIds)
    {
        content += QString("/%1").arg(linkId);
    }

    return formatMessage(msgId, false,
                         MessageType::TRIP_CTRL,
                         MessageCode::ADD_TRIP, content);
}

QJsonObject
MessageFormatter::parseMessage(const QString &message)
{
    qCDebug(lcClientTruck)
        << "MessageFormatter::parseMessage:"
        << "messageLength=" << message.length();

    QJsonObject result;
    QStringList parts = message.split('/');

    if (parts.size() < 9)
    {
        qCWarning(lcClientTruck)
            << "MessageFormatter::parseMessage:"
            << "invalid format, parts=" << parts.size()
            << "(expected >= 9)";
        // Invalid message format
        result["valid"] = false;
        return result;
    }

    // Extract message parts
    result["valid"]           = true;
    result["msgId"]           = parts[0].toInt();
    result["acknowledgement"] = (parts[1] == "1");
    result["messageType"]     = parts[2].toInt();
    result["messageCode"]     = parts[3].toInt();

    // Extract content
    QString content;
    for (int i = 8; i < parts.size(); ++i)
    {
        if (parts[i] == "-1")
        {
            break;
        }

        if (i > 8)
        {
            content += "/";
        }

        content += parts[i];
    }

    result["content"] = content;

    return result;
}

QJsonObject
MessageFormatter::parseTripInfo(const QString &message)
{
    qCDebug(lcClientTruck)
        << "MessageFormatter::parseTripInfo:"
        << "messageLength=" << message.length();

    QJsonObject parsed = parseMessage(message);

    if (!parsed["valid"].toBool()
        || parsed["messageType"].toInt()
               != static_cast<int>(MessageType::TRIPS_INFO)
        || parsed["messageCode"].toInt()
               != static_cast<int>(MessageCode::TRIP_INFO))
    {
        qCDebug(lcClientTruck)
            << "MessageFormatter::parseTripInfo:"
            << "not a valid trip info message";
        // Not a valid trip info message
        return QJsonObject();
    }

    // For trip info, the content is usually JSON
    QString       content = parsed["content"].toString();
    QJsonDocument doc =
        QJsonDocument::fromJson(content.toUtf8());

    if (doc.isNull() || !doc.isObject())
    {
        qCWarning(lcClientTruck)
            << "MessageFormatter::parseTripInfo:"
            << "failed to parse JSON content";
        return QJsonObject();
    }

    return doc.object();
}

QJsonObject
MessageFormatter::parseTripEnd(const QString &message)
{
    qCDebug(lcClientTruck)
        << "MessageFormatter::parseTripEnd:"
        << "messageLength=" << message.length();

    QJsonObject parsed = parseMessage(message);

    if (!parsed["valid"].toBool()
        || parsed["messageType"].toInt()
               != static_cast<int>(MessageType::TRIPS_INFO)
        || parsed["messageCode"].toInt()
               != static_cast<int>(MessageCode::TRIP_END))
    {
        qCDebug(lcClientTruck)
            << "MessageFormatter::parseTripEnd:"
            << "not a valid trip end message";
        // Not a valid trip end message
        return QJsonObject();
    }

    // For trip end, the content is usually JSON
    QString       content = parsed["content"].toString();
    QJsonDocument doc =
        QJsonDocument::fromJson(content.toUtf8());

    if (doc.isNull() || !doc.isObject())
    {
        qCWarning(lcClientTruck)
            << "MessageFormatter::parseTripEnd:"
            << "failed to parse JSON content";
        return QJsonObject();
    }

    return doc.object();
}

} // namespace TruckClient
} // namespace Backend
} // namespace CargoNetSim
