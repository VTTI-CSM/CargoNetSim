#include "RouteMetricUnits.h"

#include "Backend/Commons/Units.h"
#include "PathMetrics.h"

#include <QString>
#include <QStringList>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{
namespace RouteMetricUnits
{

namespace
{

const QStringList &keys()
{
    static const QStringList kKeys = {
        QStringLiteral("cost"),
        QStringLiteral("travelTime"),
        QStringLiteral("distance"),
        QStringLiteral("carbonEmissions"),
        QStringLiteral("risk"),
        QStringLiteral("energyConsumption")};
    return kKeys;
}

double toCanonicalValue(const QString &key, double value,
                        bool riskSerializedAsPercent)
{
    if (key == QStringLiteral("travelTime"))
        return Units::toSeconds(Units::hours(value)).value();
    if (key == QStringLiteral("distance"))
        return Units::toMeters(Units::kilometers(value)).value();
    if (key == QStringLiteral("risk") && riskSerializedAsPercent)
        return value / 100.0;
    return value;
}

double toDisplayValue(const QString &key, double value)
{
    if (key == QStringLiteral("travelTime"))
        return Units::toHours(Units::seconds(value)).value();
    if (key == QStringLiteral("distance"))
        return Units::toKilometers(Units::meters(value)).value();
    return value;
}

QVariantMap remapNumericValues(const QVariantMap &properties,
                               bool               toCanonical,
                               bool riskSerializedAsPercent)
{
    QVariantMap out = properties;
    for (const QString &key : keys())
    {
        auto it = out.find(key);
        if (it == out.end() || !it.value().canConvert<double>())
            continue;

        const double raw = it.value().toDouble();
        it.value() = toCanonical
            ? toCanonicalValue(key, raw, riskSerializedAsPercent)
            : toDisplayValue(key, raw);
    }
    return out;
}

} // namespace

QStringList routeMetricKeys()
{
    return keys();
}

QStringList missingCanonicalRouteMetricKeys(
    const QVariantMap &properties)
{
    QStringList missing;
    for (const QString &key : keys())
    {
        if (!properties.contains(key)
            || !properties.value(key).canConvert<double>())
        {
            missing.append(key);
        }
    }
    return missing;
}

QVariantMap defaultCanonicalProperties()
{
    QVariantMap props;
    for (const QString &key : keys())
        props.insert(key, 0.0);
    return props;
}

QVariantMap completeCanonicalProperties(
    const QVariantMap &canonicalProperties)
{
    QVariantMap props = defaultCanonicalProperties();
    for (auto it = canonicalProperties.constBegin();
         it != canonicalProperties.constEnd(); ++it)
    {
        props.insert(it.key(), it.value());
    }
    return props;
}

QVariantMap canonicalPropertiesFromDisplay(
    const QVariantMap &displayProperties)
{
    return remapNumericValues(displayProperties, true, false);
}

QVariantMap displayPropertiesFromCanonical(
    const QVariantMap &canonicalProperties)
{
    return remapNumericValues(canonicalProperties, false, false);
}

QVariantMap canonicalPropertiesFromSerialized(
    const QVariantMap &serializedProperties,
    int                schemaVersion)
{
    Q_UNUSED(schemaVersion);
    return remapNumericValues(serializedProperties, true, false);
}

QVariantMap serializedPropertiesFromCanonical(
    const QVariantMap &canonicalProperties)
{
    return remapNumericValues(canonicalProperties, false, false);
}

QVariantMap canonicalPropertiesFromMetrics(
    const PathMetrics &metrics)
{
    QVariantMap displayProperties;
    displayProperties.insert(QStringLiteral("cost"), 0.0);
    displayProperties.insert(QStringLiteral("travelTime"),
                             metrics.travelTimeHours);
    displayProperties.insert(QStringLiteral("distance"),
                             metrics.distanceKm);
    displayProperties.insert(QStringLiteral("carbonEmissions"),
                             metrics.carbonPerVehicle);
    displayProperties.insert(QStringLiteral("risk"),
                             metrics.riskPerVehicle);
    displayProperties.insert(QStringLiteral("energyConsumption"),
                             metrics.energyPerVehicle);
    return completeCanonicalProperties(
        canonicalPropertiesFromDisplay(displayProperties));
}

QJsonObject routeAttributesFromCanonical(
    const QVariantMap &canonicalProperties)
{
    QJsonObject attrs;
    for (const QString &key : keys())
    {
        const auto it = canonicalProperties.constFind(key);
        if (it == canonicalProperties.constEnd()
            || !it.value().canConvert<double>())
        {
            continue;
        }
        attrs.insert(key, it.value().toDouble());
    }
    return attrs;
}

bool isRouteMetricKey(const QString &key)
{
    return keys().contains(key);
}

} // namespace RouteMetricUnits
} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
