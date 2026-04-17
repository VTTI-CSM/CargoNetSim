#include "TransportationMode.h"
#include <stdexcept>

#include "Backend/Commons/LogCategories.h"

namespace CargoNetSim
{
namespace Backend
{

ContainerCore::Container::HaulerType
TransportationTypes::toContainerHauler(
    TransportationMode mode)
{
    qCDebug(lcModel) << "TransportationTypes::toContainerHauler: mode=" << static_cast<int>(mode);
    switch (mode)
    {
    case TransportationMode::Ship:
        return ContainerCore::Container::HaulerType::
            waterTransport;
    case TransportationMode::Truck:
        return ContainerCore::Container::HaulerType::truck;
    case TransportationMode::Train:
        return ContainerCore::Container::HaulerType::train;
    default:
        qCCritical(lcModel) << "TransportationTypes::toContainerHauler:"
                            << "invalid mode=" << static_cast<int>(mode);
        throw std::invalid_argument(
            "Invalid transportation mode");
    }
}

TransportationTypes::TransportationMode
TransportationTypes::fromContainerHauler(
    ContainerCore::Container::HaulerType hauler)
{
    qCDebug(lcModel) << "TransportationTypes::fromContainerHauler: hauler=" << static_cast<int>(hauler);
    switch (hauler)
    {
    case ContainerCore::Container::HaulerType::
        waterTransport:
        return TransportationMode::Ship;
    case ContainerCore::Container::HaulerType::truck:
        return TransportationMode::Truck;
    case ContainerCore::Container::HaulerType::train:
        return TransportationMode::Train;
    default:
        qCCritical(lcModel) << "TransportationTypes::fromContainerHauler:"
                            << "invalid hauler=" << static_cast<int>(hauler);
        throw std::invalid_argument(
            "Invalid container hauler");
    }
}

TransportationTypes::TransportationMode
TransportationTypes::fromInt(int value)
{
    qCDebug(lcModel) << "TransportationTypes::fromInt: value=" << value;
    switch (value)
    {
    case static_cast<int>(TransportationMode::Ship):
        return TransportationMode::Ship;
    case static_cast<int>(TransportationMode::Truck):
        return TransportationMode::Truck;
    case static_cast<int>(TransportationMode::Train):
        return TransportationMode::Train;
    default:
        qCCritical(lcModel) << "TransportationTypes::fromInt:"
                            << "invalid value=" << value;
        throw std::invalid_argument(
            "Invalid transportation mode value");
    }
}

QString
TransportationTypes::toString(TransportationMode mode)
{
    const QMetaEnum metaEnum =
        QMetaEnum::fromType<TransportationMode>();
    QString result = QString(
        metaEnum.valueToKey(static_cast<int>(mode)));
    qCDebug(lcModel) << "TransportationTypes::toString: mode=" << static_cast<int>(mode)
                     << "result=" << result;
    return result;
}

int TransportationTypes::toInt(TransportationMode mode)
{
    int result = static_cast<int>(mode);
    qCDebug(lcModel) << "TransportationTypes::toInt: mode -> " << result;
    return result;
}

TransportationTypes::TransportationMode
TransportationTypes::fromString(const QString &str)
{
    qCDebug(lcModel) << "TransportationTypes::fromString: str=" << str;
    // Convert to lowercase for case-insensitive comparison
    QString lowerStr = str.toLower().trimmed();

    if (lowerStr == "ship")
    {
        return TransportationMode::Ship;
    }
    else if (lowerStr == "truck")
    {
        return TransportationMode::Truck;
    }
    else if (lowerStr == "train" || lowerStr == "rail")
    {
        return TransportationMode::Train;
    }
    else
    {
        qCCritical(lcModel) << "TransportationTypes::fromString:"
                            << "invalid string:" << str;
        throw std::invalid_argument(
            "Invalid transportation mode string");
    }
}

QString transportationModeToString(
    TransportationTypes::TransportationMode m)
{
    QString result;
    switch (m)
    {
    case TransportationTypes::TransportationMode::Ship:
        result = QStringLiteral("ship");
        break;
    case TransportationTypes::TransportationMode::Truck:
        result = QStringLiteral("truck");
        break;
    case TransportationTypes::TransportationMode::Train:
        result = QStringLiteral("rail");
        break;
    case TransportationTypes::TransportationMode::Any:
    default:
        result = QString();
        break;
    }
    qCDebug(lcModel) << "transportationModeToString: mode=" << static_cast<int>(m)
                     << "result=" << result;
    return result;
}

TransportationTypes::TransportationMode
transportationModeFromString(const QString &s, bool *ok)
{
    qCDebug(lcModel) << "transportationModeFromString: s=" << s;
    const QString lower = s.trimmed().toLower();
    if (ok) *ok = true;
    if (lower == QLatin1String("ship"))
        return TransportationTypes::TransportationMode::Ship;
    if (lower == QLatin1String("truck"))
        return TransportationTypes::TransportationMode::Truck;
    if (lower == QLatin1String("rail")
        || lower == QLatin1String("train"))
        return TransportationTypes::TransportationMode::Train;
    qCWarning(lcModel) << "transportationModeFromString: unknown mode string:" << s;
    if (ok) *ok = false;
    return TransportationTypes::TransportationMode::Any;
}

QString interfaceModeCanonicalString(
    TransportationTypes::TransportationMode m)
{
    QString result;
    switch (m)
    {
    case TransportationTypes::TransportationMode::Truck:
        result = QStringLiteral("Truck");
        break;
    case TransportationTypes::TransportationMode::Train:
        result = QStringLiteral("Rail");
        break;
    case TransportationTypes::TransportationMode::Ship:
        result = QStringLiteral("Ship");
        break;
    case TransportationTypes::TransportationMode::Any:
    default:
        result = QString();
        break;
    }
    qCDebug(lcModel) << "interfaceModeCanonicalString: mode=" << static_cast<int>(m)
                     << "result=" << result;
    return result;
}

TransportationTypes::TransportationMode
interfaceModeFromCanonicalString(const QString &s, bool *ok)
{
    qCDebug(lcModel) << "interfaceModeFromCanonicalString: s=" << s;
    // Canonical-string vocabulary ("Truck"/"Rail"/"Ship") differs from the
    // YAML Connection/GlobalLink lowercase vocabulary only in case —
    // transportationModeFromString already lowercases, so we can delegate.
    return transportationModeFromString(s, ok);
}

} // namespace Backend
} // namespace CargoNetSim
