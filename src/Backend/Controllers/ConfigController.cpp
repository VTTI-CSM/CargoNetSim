#include "ConfigController.h"
#include "Backend/Commons/TransportationMode.h"
#include <QDebug>
#include <QFile>
#include <QTextStream>

namespace CargoNetSim
{
namespace Backend
{
ConfigController::ConfigController(
    const QString &configFile)
    : m_configFile(configFile)
{
    // If file doesn't exist, create it with default values
    QFile file(m_configFile);
    if (!file.exists())
    {
        createDefaultConfig();
        saveConfig();
    }
    else
    {
        loadConfig();
    }
}

bool ConfigController::loadConfig()
{
    try
    {
        QFile file(m_configFile);
        if (!file.open(QIODevice::ReadOnly
                       | QIODevice::Text))
        {
            qWarning() << "Could not open config file:"
                       << file.errorString();
            return false;
        }

        QDomDocument doc;

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        auto result = doc.setContent(&file);
        if (!result)
        {
            file.close();
            qWarning() << "Failed to parse XML:" << result.errorMessage
                       << " at line " << result.errorLine
                       << ", column " << result.errorColumn;
            return false;
        }
#else
        QString errorMsg;
        int     errorLine, errorColumn;

        if (!doc.setContent(&file, &errorMsg, &errorLine,
                            &errorColumn))
        {
            file.close();
            qWarning() << "Failed to parse XML:" << errorMsg
                       << " at line " << errorLine
                       << ", column " << errorColumn;
            return false;
        }
#endif
        file.close();

        // Get the root element
        QDomElement root = doc.documentElement();
        if (root.tagName() != "config")
        {
            qWarning() << "Invalid root element in config "
                          "file, expected 'config'";
            return false;
        }

        // Clear existing config
        m_config.clear();

        // Process each child element of the root
        QDomElement element = root.firstChildElement();
        while (!element.isNull())
        {
            QString tagName = element.tagName();

            if (tagName == "simulation"
                || tagName == "fuel_energy"
                || tagName == "fuel_carbon_content"
                || tagName == "fuel_prices"
                || tagName == "carbon_taxes"
                || tagName == "time_value_of_money")
            {

                m_config[tagName] =
                    parseXmlElement(element);
            }
            else if (tagName == "transport_modes")
            {
                QVariantMap transportModes;

                QDomElement modeElement =
                    element.firstChildElement();
                while (!modeElement.isNull())
                {
                    QString modeName =
                        modeElement.tagName();
                    transportModes[modeName] =
                        parseXmlElement(modeElement);
                    modeElement =
                        modeElement.nextSiblingElement();
                }

                m_config["transport_modes"] =
                    transportModes;
            }

            element = element.nextSiblingElement();
        }

        return true;
    }
    catch (const std::exception &e)
    {
        qWarning() << "An error occurred while loading the "
                      "config file:"
                   << e.what();
        return false;
    }
}

QVariantMap ConfigController::parseXmlElement(
    const QDomElement &element) const
{
    QVariantMap result;

    QDomElement child = element.firstChildElement();
    while (!child.isNull())
    {
        QString tagName     = child.tagName();
        QString textContent = child.text();

        // Convert to appropriate type
        if (textContent == "true" || textContent == "false")
        {
            result[tagName] = (textContent == "true");
        }
        else
        {
            bool   isDouble;
            double doubleValue =
                textContent.toDouble(&isDouble);

            if (isDouble)
            {
                result[tagName] = doubleValue;
            }
            else
            {
                // If it's not a number, keep it as a string
                result[tagName] = textContent;
            }
        }

        child = child.nextSiblingElement();
    }

    return result;
}

void ConfigController::createDefaultConfig()
{
    // Create a default configuration with the values from
    // the XML example
    QVariantMap simulation;
    simulation["time_step"]           = 15;
    simulation["time_value_of_money"] = 20.43;
    simulation["use_mode_specific"]   = false;
    simulation["shortest_paths"]      = 3;
    m_config["simulation"]            = simulation;

    QVariantMap fuelEnergy;
    fuelEnergy["HFO"]       = 11.1;
    fuelEnergy["diesel_1"]  = 10.7;
    fuelEnergy["diesel_2"]  = 10.0;
    m_config["fuel_energy"] = fuelEnergy;

    QVariantMap fuelCarbonContent;
    fuelCarbonContent["HFO"]        = 3.15;
    fuelCarbonContent["diesel_1"]   = 2.68;
    fuelCarbonContent["diesel_2"]   = 2.68;
    m_config["fuel_carbon_content"] = fuelCarbonContent;

    QVariantMap fuelPrices;
    fuelPrices["HFO"]       = 580.0;
    fuelPrices["diesel_1"]  = 1.35;
    fuelPrices["diesel_2"]  = 1.35;
    m_config["fuel_prices"] = fuelPrices;

    QVariantMap carbonTaxes;
    carbonTaxes["rate"]             = 65;
    carbonTaxes["ship_multiplier"]  = 1.2;
    carbonTaxes["truck_multiplier"] = 1.1;
    carbonTaxes["train_multiplier"] = 1.1;
    m_config["carbon_taxes"]        = carbonTaxes;

    QVariantMap ship;
    ship["average_speed"]            = 20;
    ship["average_fuel_consumption"] = 50;
    ship["average_container_number"] = 5000;
    ship["risk_factor"]              = 0.025;
    ship["fuel_type"]                = "HFO";
    ship["time_value_of_money"]      = 13.43;

    QVariantMap train;
    train["average_speed"]            = 40;
    train["average_fuel_consumption"] = 20;
    train["average_container_number"] = 400;
    train["risk_factor"]              = 0.006;
    train["use_network"]              = true;
    train["fuel_type"]                = "diesel_1";
    train["time_value_of_money"]      = 16.43;

    QVariantMap truck;
    truck["average_speed"]            = 70;
    truck["average_fuel_consumption"] = 15;
    truck["average_container_number"] = 1;
    truck["risk_factor"]              = 0.012;
    truck["use_network"]              = false;
    truck["fuel_type"]                = "diesel_2";
    truck["time_value_of_money"]      = 31.43;

    QVariantMap transportModes;
    transportModes["ship"]      = ship;
    transportModes["rail"]      = train;
    transportModes["truck"]     = truck;
    m_config["transport_modes"] = transportModes;
}

QVariantMap ConfigController::getAllParams() const
{
    return m_config;
}

QVariantMap ConfigController::getSimulationParams() const
{
    return m_config.value("simulation").toMap();
}

QVariantMap ConfigController::getFuelEnergy() const
{
    return m_config.value("fuel_energy").toMap();
}

QVariantMap ConfigController::getFuelCarbonContent() const
{
    return m_config.value("fuel_carbon_content").toMap();
}

QVariantMap ConfigController::getFuelPrices() const
{
    return m_config.value("fuel_prices").toMap();
}

QVariantMap ConfigController::getCarbonTaxes() const
{
    return m_config.value("carbon_taxes").toMap();
}

QVariantMap ConfigController::getTimeValueOfMoney() const
{
    QVariantMap timeValueOfMoney;

    // Get the simulation settings
    QVariantMap simulationParams = getSimulationParams();

    // Get the use_mode_specific flag and average value from
    // simulation section
    timeValueOfMoney["use_mode_specific"] =
        simulationParams.value("use_mode_specific", false)
            .toBool();
    timeValueOfMoney["average"] =
        simulationParams.value("time_value_of_money", 20.43)
            .toDouble();

    // Get the transport modes
    QVariantMap transportModes = getTransportModes();

    // Extract mode-specific time values
    if (transportModes.contains("ship"))
    {
        QVariantMap shipSettings =
            transportModes["ship"].toMap();
        timeValueOfMoney["ship"] =
            shipSettings.value("time_value_of_money", 13.43)
                .toDouble();
    }
    else
    {
        timeValueOfMoney["ship"] = 13.43; // Default value
    }

    if (transportModes.contains("rail"))
    {
        QVariantMap railSettings =
            transportModes["rail"].toMap();
        timeValueOfMoney["rail"] =
            railSettings.value("time_value_of_money", 16.43)
                .toDouble();
    }
    else
    {
        timeValueOfMoney["rail"] = 16.43; // Default value
    }

    if (transportModes.contains("truck"))
    {
        QVariantMap truckSettings =
            transportModes["truck"].toMap();
        timeValueOfMoney["truck"] =
            truckSettings
                .value("time_value_of_money", 31.43)
                .toDouble();
    }
    else
    {
        timeValueOfMoney["truck"] = 31.43; // Default value
    }

    return timeValueOfMoney;
}

QVariantMap ConfigController::getTransportModes() const
{
    return m_config.value("transport_modes").toMap();
}

void ConfigController::updateConfig(
    const QVariantMap &newConfig)
{
    m_config = newConfig;
}

bool ConfigController::saveConfig()
{
    try
    {
        QDomDocument doc;
        QDomElement  root = doc.createElement("config");
        doc.appendChild(root);

        // Add a comment
        QDomComment comment = doc.createComment(
            "Configuration parameters for CargoNetSim");
        root.appendChild(comment);

        // Add simulation section
        QVariantMap simulation = getSimulationParams();
        variantMapToXmlElement(doc, root, simulation,
                               "simulation");

        // Add fuel_energy section
        QVariantMap fuelEnergy = getFuelEnergy();
        variantMapToXmlElement(doc, root, fuelEnergy,
                               "fuel_energy");

        // Add fuel_carbon_content section
        QVariantMap fuelCarbonContent =
            getFuelCarbonContent();
        variantMapToXmlElement(doc, root, fuelCarbonContent,
                               "fuel_carbon_content");

        // Add fuel_prices section
        QVariantMap fuelPrices = getFuelPrices();
        variantMapToXmlElement(doc, root, fuelPrices,
                               "fuel_prices");

        // Add carbon_taxes section
        QVariantMap carbonTaxes = getCarbonTaxes();
        variantMapToXmlElement(doc, root, carbonTaxes,
                               "carbon_taxes");

        // Add transport_modes section
        QVariantMap transportModes = getTransportModes();
        QDomElement transportModesElement =
            doc.createElement("transport_modes");
        root.appendChild(transportModesElement);

        // Add ship section
        QVariantMap ship =
            transportModes.value("ship").toMap();
        variantMapToXmlElement(doc, transportModesElement,
                               ship, "ship");

        // Add train section
        QVariantMap train =
            transportModes.value("rail").toMap();
        variantMapToXmlElement(doc, transportModesElement,
                               train, "rail");

        // Add truck section
        QVariantMap truck =
            transportModes.value("truck").toMap();
        variantMapToXmlElement(doc, transportModesElement,
                               truck, "truck");

        // Write to file
        QFile file(m_configFile);
        if (!file.open(QIODevice::WriteOnly
                       | QIODevice::Text))
        {
            qWarning() << "Could not open file for writing:"
                       << file.errorString();
            return false;
        }

        QTextStream stream(&file);
        stream << doc.toString(4); // 4-space indentation
        file.close();

        return true;
    }
    catch (const std::exception &e)
    {
        qWarning() << "Error saving config:" << e.what();
        return false;
    }
}

void ConfigController::variantMapToXmlElement(
    QDomDocument &doc, QDomElement &parentElement,
    const QVariantMap &map,
    const QString     &sectionName) const
{
    QDomElement sectionElement =
        doc.createElement(sectionName);
    parentElement.appendChild(sectionElement);

    for (auto it = map.constBegin(); it != map.constEnd();
         ++it)
    {
        QDomElement element = doc.createElement(it.key());
        QDomText    text;

        switch (it.value().typeId())
        {
        case QMetaType::Bool:
            text = doc.createTextNode(
                it.value().toBool() ? "true" : "false");
            break;

        case QMetaType::Int:
            text = doc.createTextNode(
                QString::number(it.value().toInt()));
            break;

        case QMetaType::Double:
            // Use fixed-point notation for doubles with
            // precision of 6
            text = doc.createTextNode(QString::number(
                it.value().toDouble(), 'f', 6));
            break;

        default:
            text =
                doc.createTextNode(it.value().toString());
            break;
        }

        element.appendChild(text);
        sectionElement.appendChild(element);
    }
}

QVariantMap ConfigController::getCostFunctionWeights() const
{
    // Get configuration parameters
    QVariantMap simulationParams = getSimulationParams();
    QVariantMap carbonTaxes      = getCarbonTaxes();
    QVariantMap transportModes   = getTransportModes();
    QVariantMap fuelPrices       = getFuelPrices();
    QVariantMap fuelEnergy       = getFuelEnergy();
    QVariantMap timeValueOfMoney = getTimeValueOfMoney();

    // Extract required values
    bool useModeSpecific =
        timeValueOfMoney.value("use_mode_specific", false)
            .toBool();
    double averageTimeValue =
        timeValueOfMoney.value("average", 20.43).toDouble();
    double carbonTaxRate =
        carbonTaxes.value("rate", 65.0).toDouble();

    // Create default weights
    QVariantMap defaultWeights;
    defaultWeights["cost"] =
        1.0; // USD per USD (direct cost multiplier)
    defaultWeights["travelTime"] =
        averageTimeValue; // USD per hour
    defaultWeights["distance"] =
        0.0; // USD per km (not counting directly)
    defaultWeights["carbonEmissions"] =
        carbonTaxRate / 1000.0; // USD per kg of CO2
    defaultWeights["risk"] =
        100.0; // USD per risk unit (percentage * 100)
    defaultWeights["energyConsumption"] =
        1.0; // USD per kWh (default)
    defaultWeights["terminal_delay"] =
        averageTimeValue; // USD per hour
    defaultWeights["terminal_cost"] =
        1.0; // USD per USD (direct cost multiplier)

    // Create mode-specific weights by copying default
    // weights
    QVariantMap shipWeights  = defaultWeights;
    QVariantMap trainWeights = defaultWeights;
    QVariantMap truckWeights = defaultWeights;

    // Set time values of money if mode-specific is enabled
    if (useModeSpecific)
    {
        shipWeights["travelTime"] =
            timeValueOfMoney.value("ship", 13.43)
                .toDouble();
        shipWeights["terminal_delay"] =
            timeValueOfMoney.value("ship", 13.43)
                .toDouble();

        trainWeights["travelTime"] =
            timeValueOfMoney.value("rail", 16.43)
                .toDouble();
        trainWeights["terminal_delay"] =
            timeValueOfMoney.value("rail", 16.43)
                .toDouble();

        truckWeights["travelTime"] =
            timeValueOfMoney.value("truck", 31.43)
                .toDouble();
        truckWeights["terminal_delay"] =
            timeValueOfMoney.value("truck", 31.43)
                .toDouble();
    }

    // Set carbon emission multipliers
    shipWeights["carbonEmissions"] =
        shipWeights["carbonEmissions"].toDouble()
        * carbonTaxes.value("ship_multiplier", 1.2)
              .toDouble();
    trainWeights["carbonEmissions"] =
        trainWeights["carbonEmissions"].toDouble()
        * carbonTaxes.value("train_multiplier", 1.1)
              .toDouble();
    truckWeights["carbonEmissions"] =
        truckWeights["carbonEmissions"].toDouble()
        * carbonTaxes.value("truck_multiplier", 1.1)
              .toDouble();

    // Extract transport mode data
    QVariantMap shipData =
        transportModes.value("ship").toMap();
    QVariantMap trainData =
        transportModes.value("rail").toMap();
    QVariantMap truckData =
        transportModes.value("truck").toMap();

    // Set risk factors
    shipWeights["risk"] =
        shipWeights["risk"].toDouble()
        * shipData.value("risk_factor", 0.025).toDouble();
    trainWeights["risk"] =
        trainWeights["risk"].toDouble()
        * trainData.value("risk_factor", 0.006).toDouble();
    truckWeights["risk"] =
        truckWeights["risk"].toDouble()
        * truckData.value("risk_factor", 0.012).toDouble();

    // Calculate energy costs based on fuel types and
    // calorific values Ship energy cost
    QString shipFuelType =
        shipData.value("fuel_type", "HFO").toString();
    double shipFuelPrice =
        fuelPrices
            .value(shipFuelType,
                   fuelPrices.value("HFO", 0.56))
            .toDouble();
    double shipCalorificValue =
        fuelEnergy
            .value(shipFuelType,
                   fuelEnergy.value("HFO", 11.1))
            .toDouble();

    // For HFO, convert from price per ton to price per kg
    double shipEnergyCost;

    // Fuels price is per liter, calorific value
    // is kWh/L
    shipEnergyCost =
        shipFuelPrice / shipCalorificValue; // USD per kWh

    shipWeights["energyConsumption"] = shipEnergyCost;

    // Train energy cost
    QString trainFuelType =
        trainData.value("fuel_type", "diesel_1").toString();
    double trainFuelPrice =
        fuelPrices
            .value(trainFuelType,
                   fuelPrices.value("diesel_1", 1.35))
            .toDouble();
    double trainCalorificValue =
        fuelEnergy
            .value(trainFuelType,
                   fuelEnergy.value("diesel_1", 10.7))
            .toDouble();
    double trainEnergyCost =
        trainFuelPrice / trainCalorificValue; // USD per kWh
    trainWeights["energyConsumption"] = trainEnergyCost;

    // Truck energy cost
    QString truckFuelType =
        truckData.value("fuel_type", "diesel_2").toString();
    double truckFuelPrice =
        fuelPrices
            .value(truckFuelType,
                   fuelPrices.value("diesel_2", 1.35))
            .toDouble();
    double truckCalorificValue =
        fuelEnergy
            .value(truckFuelType,
                   fuelEnergy.value("diesel_2", 10.0))
            .toDouble();
    double truckEnergyCost =
        truckFuelPrice / truckCalorificValue; // USD per kWh
    truckWeights["energyConsumption"] = truckEnergyCost;

    // Using TransportationTypes enum values for
    // transportation modes
    QVariantMap updatedWeights;
    updatedWeights["default"] = defaultWeights;
    updatedWeights[QString::number(static_cast<int>(
        TransportationTypes::TransportationMode::Ship))] =
        shipWeights;
    updatedWeights[QString::number(static_cast<int>(
        TransportationTypes::TransportationMode::Train))] =
        trainWeights;
    updatedWeights[QString::number(static_cast<int>(
        TransportationTypes::TransportationMode::Truck))] =
        truckWeights;

    return updatedWeights;
}

} // namespace Backend
} // namespace CargoNetSim
