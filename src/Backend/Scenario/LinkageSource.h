#pragma once

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

/// Whether a linkage/connection/global-link was user-authored or derived.
enum class LinkageSource
{
    Manual = 0,
    Auto   = 1,
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
