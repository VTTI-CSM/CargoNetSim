#pragma once

namespace CargoNetSim::GUI::Input {

/**
 * Unique integer identifiers for every concrete QUndoCommand subclass in this
 * subsystem. Used by QUndoCommand::id() for merge detection.
 *
 * Rule: each command's `int id() const override` returns one of these and
 * never collides with another command.
 */
namespace CommandId {

constexpr int CreateConnection              = 1001;
constexpr int DeleteConnection              = 1002;
constexpr int SetTerminalRole               = 1003;
constexpr int SetTerminalType               = 1004;
constexpr int UpdateTerminalPosition        = 1005;
constexpr int UpdateRegionLocalOrigin       = 1006;
constexpr int LinkTerminalToMapPoint        = 1007;
constexpr int UnlinkTerminal                = 1008;
constexpr int CreateTerminalAtMapPoint      = 1009;
constexpr int CreateTerminalAtPoint         = 1010;
constexpr int SetTerminalGlobalPosition     = 1011;
constexpr int DeleteItem                    = 1012;
constexpr int DeleteTerminal                = 1013;

} // namespace CommandId

} // namespace CargoNetSim::GUI::Input
