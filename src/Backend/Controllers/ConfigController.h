#pragma once
#include <QDomDocument>
#include <QDomElement>
#include <QObject>
#include <QString>
#include <QVariantMap>
namespace CargoNetSim
{
namespace Backend
{
class ConfigController : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief Constructor initializes with config file path
     * @param configFile Path to the XML configuration file
     * @param parent    Optional QObject parent for lifetime
     *                  management
     */
    explicit ConfigController(const QString &configFile,
                              QObject       *parent = nullptr);
    /**
     * @brief Loads the configuration from the specified
     * file
     * @return True if loading was successful, false
     * otherwise
     */
    bool loadConfig();
    /**
     * @brief Get all parameters from the configuration
     * @return QVariantMap containing all configuration
     * parameters
     */
    QVariantMap getAllParams() const;
    /**
     * @brief Get simulation parameters
     * @return QVariantMap containing simulation parameters
     */
    QVariantMap getSimulationParams() const;
    /**
     * @brief Get fuel energy values
     * @return QVariantMap containing fuel energy values
     */
    QVariantMap getFuelEnergy() const;
    /**
     * @brief Get fuel carbon content values
     * @return QVariantMap containing fuel carbon content
     * values
     */
    QVariantMap getFuelCarbonContent() const;
    /**
     * @brief Get fuel prices
     * @return QVariantMap containing fuel prices
     */
    QVariantMap getFuelPrices() const;
    /**
     * @brief Get carbon taxes
     * @return QVariantMap containing carbon taxes
     */
    QVariantMap getCarbonTaxes() const;
    /**
     * @brief Get time value of money settings
     * @return QVariantMap containing time value of money
     * settings
     */
    QVariantMap getTimeValueOfMoney() const;
    /**
     * @brief Get transport mode parameters
     * @return QVariantMap containing transport mode
     * parameters
     */
    QVariantMap getTransportModes() const;
    /**
     * @brief Get cost function weights for different
     * transportation modes
     * @return QVariantMap containing weights for all
     * transportation modes
     *
     * Each weight represents the cost factor for the
     * corresponding parameter:
     * - cost: USD per USD (multiplier = 1.0)
     * - travellTime: USD per second
     * - distance: USD per metre (set to 0.0 as this cost is
     * accounted for elsewhere)
     * - carbonEmissions: USD per tonne of CO2
     * - risk: USD per unit of risk fraction
     * - energyConsumption: USD per kWh
     * - terminal_delay: USD per second
     * - terminal_cost: USD per USD (multiplier = 1.0)
     */
    QVariantMap getCostFunctionWeights() const;
    /**
     * @brief Update configuration in memory
     * @param newConfig New configuration to replace the
     * current one
     */
    void updateConfig(const QVariantMap &newConfig);
    /**
     * @brief Save configuration to file
     * @return True if save was successful, false otherwise
     */
    bool saveConfig();

private:
    /**
     * @brief Create default configuration with all required
     * parameters
     */
    void createDefaultConfig();
    /**
     * @brief Helper method to parse XML element into
     * QVariantMap
     * @param element The DOM element to parse
     * @return QVariantMap representation of the XML
     * element's children
     */
    QVariantMap
    parseXmlElement(const QDomElement &element) const;
    /**
     * @brief Helper method to convert QVariantMap to XML
     * element
     * @param doc The DOM document
     * @param parentElement The parent element to add
     * children to
     * @param map The QVariantMap to convert
     * @param sectionName The name of the section
     */
    void variantMapToXmlElement(
        QDomDocument &doc, QDomElement &parentElement,
        const QVariantMap &map,
        const QString     &sectionName) const;
    QString
        m_configFile; ///< Path to the configuration file
    QVariantMap m_config; ///< Loaded configuration
};
} // namespace Backend
} // namespace CargoNetSim
