#pragma once

#include <QString>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

/// How a region resolves its linkage / connection / global-link set.
enum class LinkageStrategy
{
    Manual = 0,
    Auto   = 1,
    Hybrid = 2,
};

/// Stringify for YAML serialization. Named `linkageStrategyToString` (not the
/// shorter `toString`) because Qt's `QTest::toString` template uses ADL to
/// find `toString(T)` overloads in the enum's namespace — if we provided a
/// `toString(LinkageStrategy)` returning QString, QCOMPARE failure messages
/// would trigger a compile error ("cannot convert QString to const char*").
inline QString linkageStrategyToString(LinkageStrategy s)
{
    switch (s)
    {
    case LinkageStrategy::Manual: return QStringLiteral("manual");
    case LinkageStrategy::Auto:   return QStringLiteral("auto");
    case LinkageStrategy::Hybrid: return QStringLiteral("hybrid");
    }
    return QStringLiteral("manual");
}

inline LinkageStrategy linkageStrategyFromString(const QString &s, bool *ok = nullptr)
{
    const QString lower = s.trimmed().toLower();
    if (ok) *ok = true;
    if (lower == QLatin1String("manual")) return LinkageStrategy::Manual;
    if (lower == QLatin1String("auto"))   return LinkageStrategy::Auto;
    if (lower == QLatin1String("hybrid")) return LinkageStrategy::Hybrid;
    if (ok) *ok = false;
    return LinkageStrategy::Manual;
}

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
