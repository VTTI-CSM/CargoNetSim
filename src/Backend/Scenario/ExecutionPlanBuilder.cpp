#include "ExecutionPlanBuilder.h"

#include "Backend/Commons/TransportationMode.h"
#include "Backend/Commons/Units.h"
#include "Backend/Models/Path.h"
#include "Backend/Models/PathSegment.h"
#include "NetworkSpec.h"
#include "ScenarioDocument.h"

#include <QSet>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

namespace
{

struct ResolvedSegmentTarget
{
    bool    ok = false;
    QString errorMessage;
    QString networkName;
    int     runtimeStartNodeId = -1;
    int     runtimeEndNodeId = -1;
};

QString pathOriginId(const CargoNetSim::Backend::Path &path)
{
    if (!path.getOriginId().isEmpty())
        return path.getOriginId();

    const auto segments = path.getSegments();
    return segments.isEmpty() || !segments.first()
               ? QString()
               : segments.first()->getStart();
}

QString pathDestinationId(const CargoNetSim::Backend::Path &path)
{
    if (!path.getDestinationId().isEmpty())
        return path.getDestinationId();

    const auto segments = path.getSegments();
    return segments.isEmpty() || !segments.last()
               ? QString()
               : segments.last()->getEnd();
}

QList<QPair<NodeLinkage, NodeLinkage>> intersectLinkagesByNetwork(
    const QList<NodeLinkage> &startLinkages,
    const QList<NodeLinkage> &endLinkages)
{
    QList<QPair<NodeLinkage, NodeLinkage>> candidates;
    for (const auto &start : startLinkages)
    {
        for (const auto &end : endLinkages)
        {
            if (start.networkName == end.networkName
                && start.nodeId != end.nodeId)
            {
                candidates.append(qMakePair(start, end));
            }
        }
    }
    return candidates;
}

ResolvedSegmentTarget resolveLandSegmentTarget(
    const ScenarioDocument                     &document,
    const QString                              &startTerminalId,
    const QString                              &endTerminalId,
    NetworkSpec::Type                           networkType,
    TransportationTypes::TransportationMode     mode)
{
    ResolvedSegmentTarget target;

    const auto startLinkages =
        document.linkagesFor(startTerminalId, networkType);
    const auto endLinkages =
        document.linkagesFor(endTerminalId, networkType);
    const auto candidates =
        intersectLinkagesByNetwork(startLinkages, endLinkages);

    if (candidates.isEmpty())
    {
        target.errorMessage = QStringLiteral(
            "No %1 linkage path could be resolved between %2 and %3")
                                  .arg(transportationModeToString(mode),
                                       startTerminalId, endTerminalId);
        return target;
    }

    if (candidates.size() > 1)
    {
        target.errorMessage = QStringLiteral(
            "Ambiguous %1 linkage path between %2 and %3")
                                  .arg(transportationModeToString(mode),
                                       startTerminalId, endTerminalId);
        return target;
    }

    const auto &pair         = candidates.first();
    target.ok                = true;
    target.networkName       = pair.first.networkName;
    target.runtimeStartNodeId = pair.first.nodeId;
    target.runtimeEndNodeId   = pair.second.nodeId;
    return target;
}

ResolvedSegmentTarget resolveShipSegmentTarget(
    const ScenarioDocument &document,
    const QString          &startTerminalId,
    const QString          &endTerminalId)
{
    ResolvedSegmentTarget target;
    const auto startIt = document.terminals.constFind(startTerminalId);
    const auto endIt   = document.terminals.constFind(endTerminalId);
    if (startIt == document.terminals.constEnd()
        || endIt == document.terminals.constEnd())
    {
        target.errorMessage = QStringLiteral(
            "Ship segment terminals must exist in the scenario document");
        return target;
    }

    const QString startRegion = startIt->region;
    const QString endRegion   = endIt->region;
    if (startRegion.isEmpty() || endRegion.isEmpty())
    {
        target.errorMessage = QStringLiteral(
            "Ship segment regions must be defined for %1 and %2")
                                  .arg(startTerminalId, endTerminalId);
        return target;
    }

    target.ok = true;
    target.networkName = startRegion == endRegion
                             ? startRegion
                             : startRegion + QStringLiteral("_to_")
                                   + endRegion;
    return target;
}

ResolvedSegmentTarget resolveSegmentTarget(
    const ScenarioDocument                 &document,
    const CargoNetSim::Backend::PathSegment &segment)
{
    using Mode = TransportationTypes::TransportationMode;

    switch (segment.getMode())
    {
    case Mode::Train:
        return resolveLandSegmentTarget(document, segment.getStart(),
                                        segment.getEnd(),
                                        NetworkSpec::Type::Rail,
                                        segment.getMode());
    case Mode::Truck:
        return resolveLandSegmentTarget(document, segment.getStart(),
                                        segment.getEnd(),
                                        NetworkSpec::Type::Truck,
                                        segment.getMode());
    case Mode::Ship:
        return resolveShipSegmentTarget(document, segment.getStart(),
                                        segment.getEnd());
    default:
        return { false,
                 QStringLiteral("Unsupported transportation mode on segment %1")
                     .arg(segment.getPathSegmentId()) };
    }
}

void markPlanningFailure(PathExecutionPlan &pathPlan,
                         const QString     &message)
{
    pathPlan.disposition = PlannedPathDisposition::PlanningFailure;
    pathPlan.planningMessage = message;
    pathPlan.segments.clear();
    pathPlan.transitions.clear();
}

void finalizeDisposition(PathExecutionPlan    &pathPlan,
                         ExecutionDemandPolicy demandPolicy)
{
    if (pathPlan.disposition == PlannedPathDisposition::PlanningFailure)
    {
        if (pathPlan.planningMessage.isEmpty())
        {
            pathPlan.planningMessage =
                QStringLiteral("Path planning failed");
        }
        return;
    }

    if (pathPlan.effectiveContainerCount <= 0)
    {
        pathPlan.disposition = PlannedPathDisposition::SkipNoDemand;
        pathPlan.planningMessage =
            demandPolicy
                    == ExecutionDemandPolicy::
                        DuplicateDemandPerSelectedPath
                ? QStringLiteral(
                      "No selected OD demand under the DuplicateDemandPerSelectedPath execution policy")
                : QStringLiteral(
                      "No allocated demand under the AllocatedOnly execution policy");
        return;
    }

    pathPlan.disposition = PlannedPathDisposition::Execute;
    pathPlan.planningMessage.clear();
}

ExecutionSchedulingPolicy defaultSchedulingPolicy(
    ExecutionDemandPolicy demandPolicy)
{
    switch (demandPolicy)
    {
    case ExecutionDemandPolicy::DuplicateDemandPerSelectedPath:
        return ExecutionSchedulingPolicy::SequentialPaths;
    case ExecutionDemandPolicy::AllocatedOnly:
    case ExecutionDemandPolicy::SplitAcrossSelectedPaths:
        return ExecutionSchedulingPolicy::ConcurrentPaths;
    }

    return ExecutionSchedulingPolicy::ConcurrentPaths;
}

} // namespace

ExecutionPlanBuilder::ExecutionPlanBuilder(
    const ScenarioDocument &document)
    : m_document(document)
{
}

ExecutionPlanBuildResult ExecutionPlanBuilder::build(
    const QList<CargoNetSim::Backend::Path *> &paths,
    const QVector<QString>                    &executionPathKeys,
    const PathAllocation                      &allocation,
    const QString                             &executionId,
    ExecutionDemandPolicy                      demandPolicy) const
{
    ExecutionPlanBuildResult result;

    if (executionId.isEmpty())
    {
        result.errorMessage =
            QStringLiteral("Execution plan requires a non-empty execution ID");
        return result;
    }

    if (demandPolicy == ExecutionDemandPolicy::SplitAcrossSelectedPaths)
    {
        result.errorMessage = QStringLiteral(
            "SplitAcrossSelectedPaths execution demand policy is not implemented");
        return result;
    }

    if (paths.size() != executionPathKeys.size())
    {
        result.errorMessage = QStringLiteral(
            "Selected path count must match selected execution path key count");
        return result;
    }

    QSet<QString> seenIdentities;
    QSet<QString> seenCanonicalPathKeys;
    ScenarioExecutionPlan plan;
    plan.executionId  = executionId;
    plan.demandPolicy = demandPolicy;
    plan.schedulingPolicy = defaultSchedulingPolicy(demandPolicy);
    plan.paths.reserve(executionPathKeys.size());

    for (int i = 0; i < paths.size(); ++i)
    {
        auto *path = paths[i];
        const QString executionPathKey = executionPathKeys[i];

        if (!path)
        {
            result.errorMessage = QStringLiteral(
                "Selected path at index %1 is null").arg(i);
            return result;
        }
        if (executionPathKey.isEmpty())
        {
            result.errorMessage = QStringLiteral(
                "Selected execution path key at index %1 is empty").arg(i);
            return result;
        }
        if (seenIdentities.contains(executionPathKey))
        {
            result.errorMessage = QStringLiteral(
                "Selected execution path key %1 appears more than once")
                                      .arg(executionPathKey);
            return result;
        }
        seenIdentities.insert(executionPathKey);

        PathExecutionPlan pathPlan;
        pathPlan.executionPathKey = executionPathKey;
        pathPlan.canonicalPathKey = path->canonicalPathKey();
        if (pathPlan.canonicalPathKey.isEmpty())
        {
            result.errorMessage = QStringLiteral(
                "Selected path %1 does not have a canonical path key")
                                      .arg(executionPathKey);
            return result;
        }
        if (seenCanonicalPathKeys.contains(pathPlan.canonicalPathKey))
        {
            result.errorMessage = QStringLiteral(
                "Canonical path key %1 appears more than once in the selected execution set")
                                      .arg(pathPlan.canonicalPathKey);
            return result;
        }
        seenCanonicalPathKeys.insert(pathPlan.canonicalPathKey);
        pathPlan.pathId = path->getPathId();
        pathPlan.rank = path->getRank();
        pathPlan.pathUid = path->getPathUid();
        pathPlan.originId = pathOriginId(*path);
        pathPlan.destinationId = pathDestinationId(*path);
        pathPlan.effectiveContainerCount =
            allocation.effectiveContainerCountForPath(path);
        pathPlan.predictedTotalCostUsd = path->getTotalPathCost();
        pathPlan.predictedEdgeCostUsd = path->getTotalEdgeCosts();
        pathPlan.predictedTerminalCostUsd =
            path->getTotalTerminalCosts();
        pathPlan.predictedDistanceKm =
            Units::toKilometers(
                Units::meters(path->totalEstimatedLength()))
                .value();
        pathPlan.predictedTravelTimeHours =
            Units::toHours(
                Units::seconds(path->totalEstimatedTravelTime()))
                .value();

        const auto segments = path->getSegments();
        if (segments.isEmpty())
        {
            markPlanningFailure(
                pathPlan,
                QStringLiteral("Path %1 has no segments")
                    .arg(executionPathKey));
            plan.paths.append(pathPlan);
            continue;
        }

        bool pathFailed = false;
        for (int segmentIndex = 0; segmentIndex < segments.size();
             ++segmentIndex)
        {
            auto *segment = segments[segmentIndex];
            if (!segment)
            {
                markPlanningFailure(
                    pathPlan,
                    QStringLiteral("Path %1 contains a null segment at index %2")
                        .arg(executionPathKey)
                        .arg(segmentIndex));
                pathFailed = true;
                break;
            }

            if (segmentIndex > 0)
            {
                const auto *previous = segments[segmentIndex - 1];
                if (!previous
                    || previous->getEnd() != segment->getStart())
                {
                    markPlanningFailure(
                        pathPlan,
                        QStringLiteral(
                            "Path %1 segment %2 does not continue from the previous segment")
                            .arg(executionPathKey)
                            .arg(segmentIndex));
                    pathFailed = true;
                    break;
                }
            }

            const auto resolved =
                resolveSegmentTarget(m_document, *segment);
            if (!resolved.ok)
            {
                markPlanningFailure(
                    pathPlan,
                    resolved.errorMessage);
                pathFailed = true;
                break;
            }

            SegmentExecutionPlan segmentPlan;
            segmentPlan.segmentIndex = segmentIndex;
            segmentPlan.segmentId = segment->getPathSegmentId();
            segmentPlan.executionPathKey = executionPathKey;
            segmentPlan.startTerminalId = segment->getStart();
            segmentPlan.endTerminalId = segment->getEnd();
            segmentPlan.regionName =
                m_document.terminals
                    .value(segment->getStart())
                    .region;
            segmentPlan.networkName = resolved.networkName;
            segmentPlan.runtimeStartNodeId = resolved.runtimeStartNodeId;
            segmentPlan.runtimeEndNodeId = resolved.runtimeEndNodeId;
            segmentPlan.mode = segment->getMode();
            if (segment->getMode()
                == TransportationTypes::TransportationMode::Truck)
            {
                const auto &startPlacement =
                    m_document.terminals.value(segment->getStart());
                const auto &endPlacement =
                    m_document.terminals.value(segment->getEnd());
                segmentPlan.runtimeStartLocationName =
                    startPlacement.properties
                        .value(QStringLiteral("Name"),
                               segment->getStart())
                        .toString();
                segmentPlan.runtimeEndLocationName =
                    endPlacement.properties
                        .value(QStringLiteral("Name"),
                               segment->getEnd())
                        .toString();
            }
            else if (segment->getMode()
                     == TransportationTypes::TransportationMode::Ship)
            {
                segmentPlan.startGlobalPosition =
                    m_document.globalPositionOf(
                        segment->getStart());
                segmentPlan.endGlobalPosition =
                    m_document.globalPositionOf(
                        segment->getEnd());
            }
            segmentPlan.terminalTransition.scenarioTerminalId =
                segment->getEnd();
            pathPlan.segments.append(segmentPlan);
        }

        if (!pathFailed)
        {
            const int transitionCount =
                pathPlan.segments.size() > 1
                    ? pathPlan.segments.size() - 1
                    : 0;
            pathPlan.transitions.reserve(transitionCount);
            for (int segmentIndex = 0;
                 segmentIndex + 1 < pathPlan.segments.size();
                 ++segmentIndex)
            {
                const auto &fromSegment = pathPlan.segments[segmentIndex];
                ExecutionTransition transition;
                transition.fromSegmentIndex = segmentIndex;
                transition.toSegmentIndex = segmentIndex + 1;
                transition.handoffTerminalId =
                    fromSegment.endTerminalId;
                transition.requiresTerminalHandoff =
                    fromSegment.terminalTransition.requiresTerminalHandoff;
                transition.processingRequirement =
                    fromSegment.terminalTransition.processingRequirement;
                pathPlan.transitions.append(transition);
            }
        }

        finalizeDisposition(pathPlan, demandPolicy);
        plan.paths.append(pathPlan);
    }

    result.status = ExecutionPlanBuildStatus::Success;
    result.plan   = std::move(plan);
    return result;
}

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
