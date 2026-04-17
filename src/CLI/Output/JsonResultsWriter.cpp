#include "JsonResultsWriter.h"
#include "Backend/Commons/LogCategories.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include "Backend/Scenario/PropertyKeys.h"

namespace CargoNetSim {
namespace Cli {

namespace PK = Backend::Scenario::PropertyKeys;

bool JsonResultsWriter::write(
    const QString &outputPath,
    const QList<Backend::Scenario::PathSimulationResult> &results,
    QString *err,
    const QHash<int, Backend::Scenario::PathMetrics> &metrics,
    const QHash<int, Backend::Scenario::PathKey>     &keys)
{
    qCInfo(lcCli) << "JsonResultsWriter::write: path" << outputPath
                  << "results:" << results.size()
                  << "metrics:" << metrics.size()
                  << "keys:" << keys.size();
    QJsonArray paths;
    for (const auto &r : results)
    {
        QJsonObject p;
        p[QStringLiteral("path_id")]        = r.pathId;
        p[QStringLiteral("total_cost")]     = r.totalCost;
        p[QStringLiteral("edge_costs")]     = r.edgeCosts;
        p[QStringLiteral("terminal_costs")] = r.terminalCosts;

        // Optional: (origin, destination, rank) from PathKey. Keys are
        // pure display metadata — consumers that do not care will see
        // them as unknown fields and ignore them.
        if (keys.contains(r.pathId))
        {
            const auto &k = keys.value(r.pathId);
            p[QStringLiteral("origin")]      = k.originId;
            p[QStringLiteral("destination")] = k.destinationId;
            p[QStringLiteral("rank")]        = k.rank;
        }

        // Optional: per-path metrics block. Only emitted when the
        // supplied PathMetrics is valid (mode must be supported AND
        // lookup data available). An invalid entry is equivalent to
        // the caller not supplying one at all — the block is simply
        // omitted rather than emitted as null or all-zeros.
        if (metrics.contains(r.pathId))
        {
            const auto &m = metrics.value(r.pathId);
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

        paths.append(p);
    }

    QJsonObject root;
    root[QStringLiteral("schema_version")] = 1;
    // `Qt::ISODate` on a UTC `QDateTime` emits the ISO 8601 UTC form
    // "yyyy-MM-ddTHH:mm:ssZ" with the `Z` suffix already included —
    // see the Qt::DateFormat reference. Appending `Z` manually would
    // produce `ZZ`.
    root[QStringLiteral("generated_at")] =
        QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    root[QStringLiteral("paths")] = paths;

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
