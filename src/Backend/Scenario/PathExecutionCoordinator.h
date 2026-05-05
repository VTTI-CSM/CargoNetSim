#pragma once

#include <QSet>
#include <QString>
#include <QVector>

#include "ExecutionPlanTypes.h"
#include "PathAllocation.h"
#include "SimulationDispatchTypes.h"

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

class PathExecutionCoordinator
{
public:
    bool initialize(const ScenarioExecutionPlan &plan,
                    QString                     *err = nullptr);

    const ScenarioExecutionPlan &plan() const
    {
        return m_plan;
    }

    const ExecutionLedger &ledger() const
    {
        return m_ledger;
    }

    QVector<DispatchableSegmentRef> dispatchableSegments() const
    {
        return m_dispatchableSegments;
    }

    CoordinatorSnapshot snapshot() const;

    bool seedContainers(const PathAllocation &allocation,
                        QString             *err = nullptr);

    bool registerDispatchAssignments(
        const QVector<VehicleDispatchAssignment> &assignments,
        QString                                  *err = nullptr);

    ExecutionEventOutcome markSegmentDispatched(
        const DispatchableSegmentRef &segmentRef,
        const QString                &vehicleId = QString(),
        const QString                &networkName = QString(),
        double                        eventTimeSeconds = -1.0);

    ExecutionEventOutcome applyEvent(const ExecutionEvent &event);

private:
    const PathExecutionPlan *findPathPlan(const QString &executionPathKey) const;
    PathExecutionState      *findPathState(const QString &executionPathKey);
    SegmentExecutionState   *findSegmentState(const QString &executionPathKey,
                                              int            segmentIndex);
    VehicleExecutionState   *findVehicleState(const QString &executionPathKey,
                                              int            segmentIndex,
                                              const QString &vehicleId);
    SegmentTimelineWindow   *findTimelineWindow(const QString &executionPathKey,
                                                int            segmentIndex);
    ContainerExecutionState *findContainerState(
        const QString &executionPathKey,
        const QString &logicalContainerId);

    void refreshSegmentState(const PathExecutionPlan &pathPlan,
                             SegmentExecutionState   &segmentState,
                             PathExecutionState      &pathState);
    bool segmentReadyForCompletion(const SegmentExecutionState &segmentState) const;
    QVector<DispatchableSegmentRef> recomputeDispatchableSegments() const;
    QString eventKey(const ExecutionEvent &event) const;

    ExecutionEventOutcome failPath(const ExecutionEvent &event,
                                   const QString        &message);
    ExecutionEventOutcome completeSegment(const QString &executionPathKey,
                                          int            segmentIndex,
                                          double         eventTimeSeconds);

    ScenarioExecutionPlan           m_plan;
    ExecutionLedger                 m_ledger;
    QVector<DispatchableSegmentRef> m_dispatchableSegments;
    QSet<QString>                   m_processedEventKeys;
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
