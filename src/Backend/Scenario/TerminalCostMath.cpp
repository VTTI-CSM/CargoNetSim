#include "TerminalCostMath.h"

#include <QString>
#include <QVariantMap>
#include <cmath>

#include "Backend/Commons/LogCategories.h"
#include "Backend/Models/PathSegment.h"
#include "Backend/Models/Terminal.h"
#include "PropertyKeys.h"

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

namespace PK = PropertyKeys;

namespace TerminalCostMath
{

// VERBATIM from SimulationValidationWorker.cpp:2521-2607. Do not edit
// without updating the original — both SVW and ResultsExtractor delegate
// here, so changes affect both code paths.
double dwellTime(const QJsonObject &config)
{
    qCDebug(lcScenario) << "TerminalCostMath::dwellTime: enter";
    double terminalDelay = 0.0;

    // Extract dwell time configuration
    if (config.contains("dwell_time"))
    {
        QJsonObject dwellTimeObj =
            config["dwell_time"].toObject();
        QString method =
            dwellTimeObj["method"].toString("gamma");
        QJsonObject paramsObj =
            dwellTimeObj["parameters"].toObject();

        // Convert JSON parameters to QVariantMap
        QVariantMap dwellParams;
        for (auto it = paramsObj.constBegin();
             it != paramsObj.constEnd(); ++it)
        {
            dwellParams[it.key()] = it.value().toDouble();
        }

        // Use the mean/expected value for each distribution.
        // Stored/runtime terminal delay is canonical seconds.
        if (method.compare("gamma", Qt::CaseInsensitive)
            == 0)
        {
            double shape =
                dwellParams.value("shape", 2.0).toDouble();
            double scale =
                dwellParams.value("scale", 24.0 * 3600.0)
                    .toDouble();
            terminalDelay = shape * scale;
        }
        else if (method.compare("exponential",
                                Qt::CaseInsensitive)
                 == 0)
        {
            double scale =
                dwellParams
                    .value("scale", 2.0 * 24.0 * 3600.0)
                    .toDouble();
            terminalDelay = scale;
        }
        else if (method.compare("normal",
                                Qt::CaseInsensitive)
                 == 0)
        {
            double mean =
                dwellParams
                    .value("mean", 2.0 * 24.0 * 3600.0)
                    .toDouble();
            terminalDelay = mean;
        }
        else if (method.compare("lognormal",
                                Qt::CaseInsensitive)
                 == 0)
        {
            double mean =
                dwellParams
                    .value("mean",
                           std::log(2.0 * 24.0 * 3600.0))
                    .toDouble();
            double sigma =
                dwellParams.value("sigma", 0.25).toDouble();
            terminalDelay =
                std::exp(mean + sigma * sigma / 2.0);
        }
        else
        {
            // Default to gamma distribution's expected
            // value
            double shape = 2.0;
            double scale = 24.0 * 3600.0;
            terminalDelay = shape * scale;
        }
    }

    qCDebug(lcScenario) << "TerminalCostMath::dwellTime:"
                        << "computed =" << terminalDelay << "seconds";
    return terminalDelay;
}

// VERBATIM from the original SimulationValidationWorker body. Do not
// edit without updating both SVW and ResultsExtractor — both delegate
// here, so changes affect both code paths.
bool customs(const QJsonObject &config,
             double            &customsDelay,
             double            &customsCost)
{
    qCDebug(lcScenario) << "TerminalCostMath::customs: enter";
    bool customsApplied = false;

    // Extract customs processing parameters
    if (config.contains("customs"))
    {
        QJsonObject customsObj =
            config["customs"].toObject();

        double probability =
            customsObj["probability"].toDouble(0.0);
        double delayMean =
            customsObj["delay_mean"].toDouble(0.0);

        // Apply expected customs delay (probability * mean delay).
        if (probability > 0.0 && delayMean > 0.0)
        {
            customsDelay   = probability * delayMean;
            customsApplied = true;
        }
    }

    qCDebug(lcScenario) << "TerminalCostMath::customs:"
                        << "applied=" << customsApplied
                        << "delay=" << customsDelay
                        << "cost=" << customsCost;
    return customsApplied;
}

// VERBATIM from the original SimulationValidationWorker body. Do not
// edit without updating both SVW and ResultsExtractor.
double directCosts(const QJsonObject &config, bool customsApplied)
{
    qCDebug(lcScenario) << "TerminalCostMath::directCosts: enter,"
                        << "customsApplied=" << customsApplied;
    double terminalCost = 0.0;

    // Extract cost configuration
    if (config.contains("cost"))
    {
        QJsonObject costObj = config["cost"].toObject();

        // Add fixed cost
        if (costObj.contains("fixed_fees"))
        {
            terminalCost +=
                costObj["fixed_fees"].toDouble(0.0);
        }

        // Add customs cost if applicable based on
        // probability
        if (customsApplied
            && costObj.contains("customs_fees"))
        {
            terminalCost +=
                costObj["customs_fees"].toDouble(0.0);
        }

        // Risk factor calculation would require container
        // value For simplicity, we'll use a nominal
        // container value of $1
        if (costObj.contains(PK::Mode::RiskFactor))
        {
            double riskFactor =
                costObj[PK::Mode::RiskFactor].toDouble(0.0);
            double nominalContainerValue =
                1.0; // Placeholder value
            terminalCost +=
                nominalContainerValue * riskFactor;
        }
    }

    qCDebug(lcScenario) << "TerminalCostMath::directCosts:"
                        << "computed =" << terminalCost;
    return terminalCost;
}

// Composes the three pure terminal helpers (dwellTime/customs/directCosts)
// with weight multipliers selected by transport mode.
double singleTerminalCost(
    CargoNetSim::Backend::Terminal *terminal,
    const QVariantMap              &costFunctionWeights,
    int                             containerCount,
    TransportationTypes::TransportationMode mode)
{
    if (!terminal)
    {
        return 0.0;
    }

    qCDebug(lcScenario) << "TerminalCostMath::singleTerminalCost:"
                        << "terminal =" << terminal->getNames().value(0)
                        << ", containerCount =" << containerCount;

    // Get terminal properties from the terminal object
    QJsonObject config = terminal->getConfig();

    const QString     modeKey = QString::number(static_cast<int>(mode));
    const QVariantMap weights =
        costFunctionWeights.contains(modeKey)
            ? costFunctionWeights[modeKey].toMap()
            : costFunctionWeights["default"].toMap();

    // Process terminal costs - per container
    double terminalDelayPerContainer = 0.0; // Seconds
    double terminalCostPerContainer  = 0.0; // Direct cost
    bool   customsApplied            = false;

    // Calculate dwell time
    terminalDelayPerContainer = dwellTime(config);

    // Calculate customs costs and delays
    double customsDelay = 0.0;
    double customsCost  = 0.0;
    customsApplied      = customs(config, customsDelay, customsCost);

    terminalDelayPerContainer += customsDelay;
    terminalCostPerContainer += customsCost;

    // Extract cost configuration
    terminalCostPerContainer += directCosts(config, customsApplied);

    // Calculate total terminal costs for all containers
    double totalTerminalDelay =
        terminalDelayPerContainer * containerCount;
    double totalTerminalDirectCost =
        terminalCostPerContainer * containerCount;

    // Apply weights to get the final terminal cost
    double terminalTotalCost =
        (totalTerminalDelay
         * weights[PK::Segment::TerminalDelay].toDouble())
        + (totalTerminalDirectCost
           * weights[PK::Segment::TerminalCost].toDouble());

    qCDebug(lcScenario) << "TerminalCostMath::singleTerminalCost:"
                        << "terminal" << terminal->getNames().value(0)
                        << "-> dwellDelay =" << terminalDelayPerContainer
                        << "s, directCost =" << terminalCostPerContainer
                        << ", totalCost =" << terminalTotalCost;

    return terminalTotalCost;
}

// VERBATIM from the original SimulationValidationWorker body. Do not
// edit without updating both SVW and ResultsExtractor. The commented-out
// mode-change loop is preserved for behavior parity (SVW has a trailing
// dead `return totalTerminalCosts` after the block; the active path is
// the per-segment estimated_cost split above).
double totalTerminalCosts(
    const QList<CargoNetSim::Backend::PathSegment *> &segments,
    const QList<CargoNetSim::Backend::PathTerminal>  & /*terminals*/,
    const QVariantMap & /*costFunctionWeights*/,
    int /*containerCount*/)
{
    qCDebug(lcScenario) << "TerminalCostMath::totalTerminalCosts:"
                        << "segment count =" << segments.size();

    double totalTerminalCosts = 0.0;
    int    segmentCount       = segments.size();

    // Early return for empty list
    if (segmentCount == 0)
    {
        return totalTerminalCosts;
    }

    for (int i = 0; i < segmentCount; i++)
    {
        auto attributes = segments[i]->getAttributes();
        if (!attributes.contains("estimated_cost"))
        {
            continue;
        }

        auto terminalCostsMap = attributes["estimated_cost"]
                                    .toObject()
                                    .toVariantMap();
        double previousCost =
            terminalCostsMap
                .value("previousTerminalCost", 0.0)
                .toDouble();
        double nextCost =
            terminalCostsMap.value("nextTerminalCost", 0.0)
                .toDouble();

        // Calculate costs based on segment position
        // First segment: full previous, half next
        // Last segment: half previous, full next
        // Middle segment: half of both
        // Single segment: full both
        bool isFirst = (i == 0);
        bool isLast  = (i == segmentCount - 1);

        totalTerminalCosts +=
            (isFirst ? previousCost : previousCost / 2.0)
            + (isLast ? nextCost : nextCost / 2.0);
    }

    qCDebug(lcScenario) << "TerminalCostMath::totalTerminalCosts:"
                        << "total =" << totalTerminalCosts;

    return totalTerminalCosts;
}

} // namespace TerminalCostMath
} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
