#pragma once

#include <QJsonObject>
#include <QStringList>
#include <QVariantMap>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{
namespace RouteMetricUnits
{

QStringList routeMetricKeys();
QStringList missingCanonicalRouteMetricKeys(
    const QVariantMap &properties);

QVariantMap defaultCanonicalProperties();
QVariantMap canonicalPropertiesFromDisplay(
    const QVariantMap &displayProperties);
QVariantMap displayPropertiesFromCanonical(
    const QVariantMap &canonicalProperties);
QVariantMap canonicalPropertiesFromSerialized(
    const QVariantMap &serializedProperties,
    int                schemaVersion);
QVariantMap serializedPropertiesFromCanonical(
    const QVariantMap &canonicalProperties);

QJsonObject routeAttributesFromCanonical(
    const QVariantMap &canonicalProperties);

bool isRouteMetricKey(const QString &key);

} // namespace RouteMetricUnits
} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
