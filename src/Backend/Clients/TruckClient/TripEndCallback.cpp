/**
 * @file TripEndCallback.cpp
 * @brief Implements trip end callback mechanism
 * @author Ahmed Aredah
 * @date 2025-03-22
 */

#include "TripEndCallback.h"
#include "Backend/Commons/LogCategories.h"

namespace CargoNetSim
{
namespace Backend
{
namespace TruckClient
{

TripEndCallbackManager::TripEndCallbackManager(
    QObject *parent)
    : QObject(parent)
{
}

void TripEndCallbackManager::registerGlobalCallback(
    const QString                           &callbackId,
    std::function<void(const TripEndData &)> callback)
{
    qCDebug(lcClientTruck)
        << "TripEndCallbackManager::"
           "registerGlobalCallback:"
        << "callbackId=" << callbackId;
    m_globalCallbacks[callbackId] = callback;
}

void TripEndCallbackManager::registerTripCallback(
    const QString &tripId, const QString &callbackId,
    std::function<void(const TripEndData &)> callback)
{
    qCDebug(lcClientTruck)
        << "TripEndCallbackManager::"
           "registerTripCallback:"
        << "tripId=" << tripId
        << "callbackId=" << callbackId;
    m_tripCallbacks[tripId][callbackId] = callback;
}

void TripEndCallbackManager::registerNetworkCallback(
    const QString &networkName, const QString &callbackId,
    std::function<void(const TripEndData &)> callback)
{
    qCDebug(lcClientTruck)
        << "TripEndCallbackManager::"
           "registerNetworkCallback:"
        << "networkName=" << networkName
        << "callbackId=" << callbackId;
    m_networkCallbacks[networkName][callbackId] = callback;
}

bool TripEndCallbackManager::unregisterGlobalCallback(
    const QString &callbackId)
{
    qCDebug(lcClientTruck)
        << "TripEndCallbackManager::"
           "unregisterGlobalCallback:"
        << "callbackId=" << callbackId;

    if (!m_globalCallbacks.contains(callbackId))
    {
        qCDebug(lcClientTruck)
            << "TripEndCallbackManager::"
               "unregisterGlobalCallback:"
            << "callback not found"
            << "callbackId=" << callbackId;
        return false;
    }

    m_globalCallbacks.remove(callbackId);
    return true;
}

bool TripEndCallbackManager::unregisterTripCallback(
    const QString &tripId, const QString &callbackId)
{
    qCDebug(lcClientTruck)
        << "TripEndCallbackManager::"
           "unregisterTripCallback:"
        << "tripId=" << tripId
        << "callbackId=" << callbackId;

    if (!m_tripCallbacks.contains(tripId)
        || !m_tripCallbacks[tripId].contains(callbackId))
    {
        qCDebug(lcClientTruck)
            << "TripEndCallbackManager::"
               "unregisterTripCallback:"
            << "callback not found"
            << "tripId=" << tripId
            << "callbackId=" << callbackId;
        return false;
    }

    m_tripCallbacks[tripId].remove(callbackId);

    // Clean up empty maps
    if (m_tripCallbacks[tripId].isEmpty())
    {
        m_tripCallbacks.remove(tripId);
    }

    return true;
}

bool TripEndCallbackManager::unregisterNetworkCallback(
    const QString &networkName, const QString &callbackId)
{
    qCDebug(lcClientTruck)
        << "TripEndCallbackManager::"
           "unregisterNetworkCallback:"
        << "networkName=" << networkName
        << "callbackId=" << callbackId;

    if (!m_networkCallbacks.contains(networkName)
        || !m_networkCallbacks[networkName].contains(
            callbackId))
    {
        qCDebug(lcClientTruck)
            << "TripEndCallbackManager::"
               "unregisterNetworkCallback:"
            << "callback not found"
            << "networkName=" << networkName
            << "callbackId=" << callbackId;
        return false;
    }

    m_networkCallbacks[networkName].remove(callbackId);

    // Clean up empty maps
    if (m_networkCallbacks[networkName].isEmpty())
    {
        m_networkCallbacks.remove(networkName);
    }

    return true;
}

void TripEndCallbackManager::unregisterAllCallbacks()
{
    qCDebug(lcClientTruck)
        << "TripEndCallbackManager::"
           "unregisterAllCallbacks:"
        << "global=" << m_globalCallbacks.size()
        << "trip=" << m_tripCallbacks.size()
        << "network=" << m_networkCallbacks.size();
    m_globalCallbacks.clear();
    m_tripCallbacks.clear();
    m_networkCallbacks.clear();
}

void TripEndCallbackManager::onTripEnded(
    const TripEndData &data)
{
    qCDebug(lcClientTruck)
        << "TripEndCallbackManager::onTripEnded:"
        << "tripId=" << data.tripId
        << "network=" << data.networkName;

    // Emit the signal for QObject connections
    emit tripEnded(data);

    int callbacksFired = 0;

    // Execute global callbacks
    for (const auto &callback : m_globalCallbacks)
    {
        callback(data);
        ++callbacksFired;
    }

    // Execute trip-specific callbacks
    if (m_tripCallbacks.contains(data.tripId))
    {
        for (const auto &callback :
             m_tripCallbacks[data.tripId])
        {
            callback(data);
            ++callbacksFired;
        }
    }

    // Execute network-specific callbacks
    if (m_networkCallbacks.contains(data.networkName))
    {
        for (const auto &callback :
             m_networkCallbacks[data.networkName])
        {
            callback(data);
            ++callbacksFired;
        }
    }

    qCDebug(lcClientTruck)
        << "TripEndCallbackManager::onTripEnded:"
        << "callbacksFired=" << callbacksFired
        << "tripId=" << data.tripId;
}

} // namespace TruckClient
} // namespace Backend
} // namespace CargoNetSim
