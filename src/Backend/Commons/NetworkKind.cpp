#include "NetworkKind.h"
#include "Backend/Commons/LogCategories.h"

namespace CargoNetSim
{
namespace Backend
{

QString networkKindToString(NetworkKind k)
{
    switch (k)
    {
    case NetworkKind::Rail:  return QStringLiteral("rail");
    case NetworkKind::Truck: return QStringLiteral("truck");
    }
    qCWarning(lcModel) << "networkKindToString: unrecognized NetworkKind"
                        << static_cast<int>(k) << "— defaulting to \"rail\"";
    return QStringLiteral("rail");
}

NetworkKind networkKindFromString(const QString &s, bool *ok)
{
    const QString lower = s.trimmed().toLower();
    if (ok) *ok = true;
    if (lower == QLatin1String("rail"))  return NetworkKind::Rail;
    if (lower == QLatin1String("truck")) return NetworkKind::Truck;
    if (ok) *ok = false;
    qCWarning(lcModel) << "networkKindFromString: unrecognized string"
                        << s << "— defaulting to Rail";
    return NetworkKind::Rail;
}

} // namespace Backend
} // namespace CargoNetSim
