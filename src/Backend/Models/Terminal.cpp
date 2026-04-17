#include "Terminal.h"
#include "Backend/Commons/LogCategories.h"
#include <QJsonArray>
#include <stdexcept>

namespace CargoNetSim
{
namespace Backend
{

Terminal::Terminal(
    const QStringList &names, const QString &displayName,
    const QJsonObject &config,
    const QMap<
        TerminalTypes::TerminalInterface,
        QSet<TransportationTypes::TransportationMode>>
                  &interfaces,
    const QString &region, QObject *parent)
    : QObject(parent)
    , m_names(names)
    , m_displayName(displayName)
    , m_config(config)
    , m_interfaces(interfaces)
    , m_region(region)
{
    if (names.isEmpty())
    {
        throw std::invalid_argument(
            "Names list cannot be empty");
    }
}

QJsonObject Terminal::toJson() const
{
    qCDebug(lcModel) << "Terminal::toJson:" << m_displayName;
    QJsonObject json;
    json["terminal_names"] =
        QJsonArray::fromStringList(m_names);
    json["display_name"]  = m_displayName;
    json["custom_config"] = m_config;
    QJsonObject interfacesJson;
    for (auto it = m_interfaces.constBegin();
         it != m_interfaces.constEnd(); ++it)
    {
        QJsonArray modesArray;
        for (TransportationTypes::TransportationMode mode :
             it.value())
        {
            modesArray.append(static_cast<int>(mode));
        }
        interfacesJson[QString::number(
            static_cast<int>(it.key()))] = modesArray;
    }
    json["terminal_interfaces"] = interfacesJson;

    if (!m_region.isEmpty())
    {
        json["region"] = m_region;
    }
    return json;
}

Terminal *Terminal::fromJson(const QJsonObject &json)
{
    qCDebug(lcModel) << "Terminal::fromJson: parsing terminal";
    // Extract terminal names
    QStringList terminalNames;
    if (json.contains("terminal_name")
        && json["terminal_name"].isString())
    {
        terminalNames.append(
            json["terminal_name"].toString());
    }
    else if (json.contains("terminal_names")
             && json["terminal_names"].isArray())
    {
        QJsonArray namesArray =
            json["terminal_names"].toArray();
        for (const QJsonValue &name : namesArray)
        {
            if (name.isString())
            {
                terminalNames.append(name.toString());
            }
        }
    }

    if (terminalNames.isEmpty())
    {
        qCWarning(lcModel) << "Terminal::fromJson:"
                           << "missing or invalid terminal name(s)";
        return nullptr;
    }

    qCDebug(lcModel) << "Terminal::fromJson: name ="
                      << terminalNames.first();

    QString dispName;
    if (json.contains("display_name"))
    {
        if (json["display_name"].isString())
        {

            dispName = json["display_name"].toString();
        }
    }
    else
    {
        qCWarning(lcModel) << "Terminal::fromJson:"
                           << "missing display_name for"
                           << terminalNames.first();
    }

    // Extract interfaces
    QMap<TerminalTypes::TerminalInterface,
         QSet<TransportationTypes::TransportationMode>>
        interfaces;
    if (json.contains("interfaces")
        && json["interfaces"].isObject())
    {
        QJsonObject interfacesJson =
            json["interfaces"].toObject();
        for (auto it = interfacesJson.constBegin();
             it != interfacesJson.constEnd(); ++it)
        {
            bool ok;
            int  interfaceInt = it.key().toInt(&ok);
            if (!ok)
                continue;

            TerminalTypes::TerminalInterface interface =
                static_cast<
                    TerminalTypes::TerminalInterface>(
                    interfaceInt);
            QSet<TransportationTypes::TransportationMode>
                modes;

            if (it.value().isArray())
            {
                QJsonArray modesArray =
                    it.value().toArray();
                for (const QJsonValue &modeValue :
                     modesArray)
                {
                    if (modeValue.isDouble())
                    {
                        int modeInt = modeValue.toInt();
                        modes.insert(
                            static_cast<
                                TransportationTypes::
                                    TransportationMode>(
                                modeInt));
                    }
                }
            }

            interfaces[interface] = modes;
        }
    }
    else if (json.contains("terminal_interfaces")
             && json["terminal_interfaces"].isObject())
    {
        QJsonObject interfacesJson =
            json["terminal_interfaces"].toObject();
        for (auto it = interfacesJson.constBegin();
             it != interfacesJson.constEnd(); ++it)
        {
            bool ok;
            int  interfaceInt = it.key().toInt(&ok);
            if (!ok)
                continue;

            TerminalTypes::TerminalInterface interface =
                static_cast<
                    TerminalTypes::TerminalInterface>(
                    interfaceInt);
            QSet<TransportationTypes::TransportationMode>
                modes;

            if (it.value().isArray())
            {
                QJsonArray modesArray =
                    it.value().toArray();
                for (const QJsonValue &modeValue :
                     modesArray)
                {
                    if (modeValue.isDouble())
                    {
                        int modeInt = modeValue.toInt();
                        modes.insert(
                            static_cast<
                                TransportationTypes::
                                    TransportationMode>(
                                modeInt));
                    }
                }
            }

            interfaces[interface] = modes;
        }
    }

    // Extract region
    QString region;
    if (json.contains("region")
        && json["region"].isString())
    {
        region = json["region"].toString();
    }

    // Create config object with only the specific required
    // objects
    QJsonObject configJson;

    // Extract cost configuration if available
    if (json.contains("cost") && json["cost"].isObject())
    {
        configJson["cost"] = json["cost"];
    }

    // Extract customs configuration if available
    if (json.contains("customs")
        && json["customs"].isObject())
    {
        configJson["customs"] = json["customs"];
    }

    // Extract dwell_time configuration if available
    if (json.contains("dwell_time")
        && json["dwell_time"].isObject())
    {
        configJson["dwell_time"] = json["dwell_time"];
    }

    // Extract the capacity
    if (json.contains("capacity")
        && json["capacity"].isObject())
    {
        configJson["capacity"] = json["capacity"];
    }

    // extract the aliases
    if (json.contains("mode_network_aliases")
        && json["mode_network_aliases"].isObject())
    {
        configJson["mode_network_aliases"] =
            json["mode_network_aliases"];
    }

    // Create the Terminal with the extracted data
    Terminal *terminal =
        new Terminal(terminalNames, dispName, configJson,
                     interfaces, region);

    return terminal;
}

} // namespace Backend
} // namespace CargoNetSim
