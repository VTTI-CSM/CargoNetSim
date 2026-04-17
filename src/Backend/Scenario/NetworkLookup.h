#pragma once

#include <QMap>
#include <QString>

#include "Backend/Clients/TrainClient/TrainNetwork.h"
#include "Backend/Clients/TruckClient/TruckNetwork.h"
#include "Backend/Commons/NetworkKind.h"

class QObject;

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{
class ScenarioRegistry;

/**
 * @brief Shared "preview-or-live" network lookup policy.
 *
 * One implementation, two consumers:
 *   - ScenarioLinker (auto-link rule evaluation)
 *   - SimulationRequestBuilder (per-segment orchestration)
 *
 * Preferred resolution order:
 *   1. If the registry holds preview networks (CLI preview / headless
 *      path), return those.
 *   2. Otherwise walk CargoNetSimController::getInstance() →
 *      RegionDataController::getRegionData(regionName) → network.
 *
 * Stateless — free functions. No caching; callers pace the lookups.
 */
namespace NetworkLookup
{

/** @brief All rail networks reachable from @p regionName, keyed by name. */
QMap<QString, TrainClient::NeTrainSimNetwork *>
collectRail(const ScenarioRegistry &registry,
            const QString          &regionName);

/** @brief All truck networks reachable from @p regionName, keyed by name. */
QMap<QString, TruckClient::IntegrationNetwork *>
collectTruck(const ScenarioRegistry &registry,
             const QString          &regionName);

/** @brief Single rail network by name, or nullptr if not found. */
TrainClient::NeTrainSimNetwork *
findRail(const ScenarioRegistry &registry,
         const QString          &regionName,
         const QString          &networkName);

/** @brief Single truck network by name, or nullptr if not found. */
TruckClient::IntegrationNetwork *
findTruck(const ScenarioRegistry &registry,
          const QString          &regionName,
          const QString          &networkName);

/**
 * @brief Reverse lookup: given a `QObject*` that is expected to be
 *        either a `NeTrainSimNetwork*` or an `IntegrationNetwork*`
 *        (typically sourced from `MapPoint::getReferenceNetwork()` or
 *        `MapLine::getReferenceNetwork()`), return the network's
 *        `getNetworkName()`.
 *
 * Centralises the "downcast to the two known land-network types and
 * pull out the name" pattern that previously appeared in ~13 GUI
 * call-sites (ViewController, UtilityFunctions, PathFindingWorker,
 * MapPointFactory). Strict DRY: one place to update if the set of
 * network subclasses grows; no per-site knowledge of which types are
 * "known" network subclasses.
 *
 * @param network The reference-network pointer from a scene item.
 *                Null is accepted and returns an empty QString.
 * @param outKind Optional out-param — set to the matching kind when a
 *                cast succeeds. Untouched when @p network is null or
 *                neither cast matches.
 * @return The network's name, or an empty QString when @p network is
 *         null / unrecognised.
 *
 * Uses `qobject_cast` so it works correctly in the presence of Qt's
 * meta-object system even when RTTI is off (both subclasses are
 * QObject + Q_OBJECT).
 */
QString networkNameOf(QObject *network, NetworkKind *outKind = nullptr);

} // namespace NetworkLookup
} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
