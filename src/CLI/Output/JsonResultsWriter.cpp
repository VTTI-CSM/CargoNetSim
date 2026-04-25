#include "JsonResultsWriter.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/Commons/TransportationMode.h"
#include "Backend/Models/Path.h"
#include "Backend/Models/PathSegment.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include "Backend/Scenario/PropertyKeys.h"

namespace CargoNetSim {
namespace Cli {

namespace PK = Backend::Scenario::PropertyKeys;

namespace
{

QJsonArray previewVehicleBreakdownToJson(
    const QList<Backend::Scenario::PathMetrics::VehicleRequirement>
        &requirements)
{
    QJsonArray json;
    for (const auto &requirement : requirements)
    {
        QJsonObject entry;
        entry[QStringLiteral("segment_index")] =
            requirement.segmentIndex;
        entry[QStringLiteral("mode")] =
            static_cast<int>(requirement.mode);
        entry[QStringLiteral("mode_name")] =
            Backend::TransportationTypes::toString(requirement.mode);
        entry[QStringLiteral("vehicles_needed")] =
            requirement.vehiclesNeeded;
        json.append(entry);
    }
    return json;
}

} // namespace

bool JsonResultsWriter::write(
    const QString &outputPath,
    const QList<Backend::Scenario::PathSimulationResult> &results,
    QString *err,
    const QHash<QString, Backend::Scenario::PathMetrics> &metrics,
    const QHash<QString, Backend::Scenario::PathKey>     &keys,
    const QList<Backend::Path *>                     &paths)
{
    qCInfo(lcCli) << "JsonResultsWriter::write: path" << outputPath
                  << "results:" << results.size()
                  << "metrics:" << metrics.size()
                  << "keys:" << keys.size()
                  << "paths:" << paths.size();

    // Build a lookup index for fast O(1) segment lookup inside the loop.
    QHash<QString, Backend::Path *> pathIndex;
    for (auto *p : paths)
        if (p) pathIndex[p->canonicalPathKey()] = p;

    QJsonArray pathsArr;
    for (const auto &r : results)
    {
        QJsonObject p;
        p[QStringLiteral("path_id")]        = r.pathId;
        if (!r.pathUid.isEmpty())
            p[QStringLiteral("path_uid")] = r.pathUid;
        p[QStringLiteral("total_cost")]     = r.totalCost;
        p[QStringLiteral("edge_costs")]     = r.edgeCosts;
        p[QStringLiteral("terminal_costs")] = r.terminalCosts;
        p[QStringLiteral("effective_container_count")] =
            r.effectiveContainerCount;

        const QString canonicalKey = !r.canonicalPathKey.isEmpty()
            ? r.canonicalPathKey
            : r.pathUid;

        // Optional: (origin, destination, rank) from PathKey. Keys are
        // pure display metadata — consumers that do not care will see
        // them as unknown fields and ignore them.
        if (keys.contains(canonicalKey))
        {
            const auto &k = keys.value(canonicalKey);
            p[QStringLiteral("origin")]      = k.originId;
            p[QStringLiteral("destination")] = k.destinationId;
            p[QStringLiteral("rank")]        = k.rank;
        }
        else
        {
            if (!r.originId.isEmpty())
                p[QStringLiteral("origin")] = r.originId;
            if (!r.destinationId.isEmpty())
                p[QStringLiteral("destination")] = r.destinationId;
            p[QStringLiteral("rank")] = r.rank;
        }

        // Optional: per-path metrics block. Only emitted when the
        // supplied PathMetrics is valid (mode must be supported AND
        // lookup data available). An invalid entry is equivalent to
        // the caller not supplying one at all — the block is simply
        // omitted rather than emitted as null or all-zeros.
        if (metrics.contains(canonicalKey))
        {
            const auto &m = metrics.value(canonicalKey);
            if (m.valid)
            {
                QJsonObject perVeh;
                perVeh[QStringLiteral("fuel")]       = m.fuelPerVehicle;
                perVeh[QStringLiteral("energy_kwh")] = m.energyPerVehicle;
                perVeh[QStringLiteral("carbon_t")]   = m.carbonPerVehicle;
                perVeh[QStringLiteral("risk")]       = m.riskPerVehicle;

                QJsonObject perCont;
                perCont[QStringLiteral("fuel")]       = m.fuelPerContainer;
                perCont[QStringLiteral("energy_kwh")] = m.energyPerContainer;
                perCont[QStringLiteral("carbon_t")]   = m.carbonPerContainer;
                perCont[QStringLiteral("risk")]       = m.riskPerContainer;

                QJsonObject mo;
                mo[QStringLiteral("preview_container_count")] =
                    m.containerCount;
                mo[QStringLiteral("preview_vehicles_needed")] =
                    m.vehiclesNeeded;
                mo[QStringLiteral("preview_vehicle_breakdown")] =
                    previewVehicleBreakdownToJson(
                        m.previewVehicleBreakdown);
                mo[QStringLiteral("container_count")] = m.containerCount;
                mo[QStringLiteral("vehicles_needed")] = m.vehiclesNeeded;
                mo[QStringLiteral("distance_km")]     = m.distanceKm;
                mo[QStringLiteral("travel_time_h")]   = m.travelTimeHours;
                mo[PK::Mode::FuelType]                = m.fuelType;
                mo[QStringLiteral("per_vehicle")]     = perVeh;
                mo[QStringLiteral("per_container")]   = perCont;

                p[QStringLiteral("metrics")] = mo;
            }
        }

        // Optional segments array — emitted when a matching Path* is available.
        if (pathIndex.contains(canonicalKey))
        {
            const auto *path = pathIndex.value(canonicalKey);

            using M = Backend::TransportationTypes::TransportationMode;
            auto modeStr = [](M m) -> QString {
                switch (m) {
                    case M::Ship:  return QStringLiteral("ship");
                    case M::Train: return QStringLiteral("rail");
                    case M::Truck: return QStringLiteral("truck");
                    default:       return QStringLiteral("unknown");
                }
            };

            QJsonArray segsArr;
            for (const auto *seg : path->getSegments())
            {
                if (!seg) continue;
                QJsonObject estimated;
                estimated[QStringLiteral("distance_m")]    = seg->estimatedDistance();
                estimated[QStringLiteral("travel_time_s")] = seg->estimatedTravelTime();
                estimated[QStringLiteral("energy_kwh")]    = seg->estimatedEnergyConsumption();
                estimated[QStringLiteral("carbon_t")]      = seg->estimatedCarbonEmissions();
                estimated[QStringLiteral("risk")]          = seg->estimatedRisk();

                QJsonObject segObj;
                segObj[QStringLiteral("segment_id")] = seg->getPathSegmentId();
                segObj[QStringLiteral("mode")]       = modeStr(seg->getMode());
                segObj[QStringLiteral("from")]       = seg->getStart();
                segObj[QStringLiteral("to")]         = seg->getEnd();
                segObj[QStringLiteral("estimated")]  = estimated;
                segsArr.append(segObj);
            }
            p[QStringLiteral("segments")] = segsArr;
        }

        pathsArr.append(p);
    }

    QJsonObject root;
    root[QStringLiteral("schema_version")] = 2;
    // `Qt::ISODate` on a UTC `QDateTime` emits the ISO 8601 UTC form
    // "yyyy-MM-ddTHH:mm:ssZ" with the `Z` suffix already included —
    // see the Qt::DateFormat reference. Appending `Z` manually would
    // produce `ZZ`.
    root[QStringLiteral("generated_at")] =
        QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    root[QStringLiteral("paths")] = pathsArr;

    // Atomic write: QSaveFile stages into a sibling tempfile, commit()
    // renames into place. A crash before commit leaves the previous
    // file (or nothing) — never a half-written result set.
    QSaveFile f(outputPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qCWarning(lcCli) << "JsonResultsWriter::write: cannot open" << outputPath
                         << f.errorString();
        if (err)
            *err = QStringLiteral("Cannot open %1: %2")
                       .arg(outputPath, f.errorString());
        return false;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!f.commit())
    {
        qCWarning(lcCli) << "JsonResultsWriter::write: cannot commit" << outputPath
                         << f.errorString();
        if (err)
            *err = QStringLiteral("Cannot commit %1: %2")
                       .arg(outputPath, f.errorString());
        return false;
    }
    qCInfo(lcCli) << "JsonResultsWriter::write: wrote" << results.size()
                  << "results to" << outputPath;
    return true;
}

} // namespace Cli
} // namespace CargoNetSim
