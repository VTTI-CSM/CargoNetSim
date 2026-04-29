/**
 * @file TruckNetwork.h
 * @brief Defines truck network model
 * @author Ahmed Aredah
 * @date 2025-03-22
 */

#pragma once

#include "Backend/Models/BaseNetwork.h"
#include "IntegrationLink.h"
#include "IntegrationLinkDataReader.h"
#include "IntegrationNode.h"
#include "IntegrationNodeDataReader.h"
#include "MessageFormatter.h"
#include "TransportationGraph.h"
#include <QJsonObject>
#include <QMutex>
#include <QObject>
#include <QSharedPointer>
#include <QString>
#include <QVector>

#include "Backend/Commons/ShortestPathResult.h"
#include "Backend/Models/BaseObject.h"

namespace CargoNetSim
{
namespace Backend
{
namespace TruckClient
{

/**
 * @class SharedIntegrationNetwork
 * @brief Represents a shared truck network model
 *
 * Manages the network structure with nodes and links,
 * providing path-finding and network operations with
 * specialized transportation attributes.
 */
class IntegrationNetwork : public BaseNetwork
{
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param parent Parent QObject
     */
    explicit IntegrationNetwork(QObject *parent = nullptr);

    /**
     * @brief Destructor
     */
    ~IntegrationNetwork();

    /**
     * @brief Initialize network with nodes and links
     * @param nodes Vector of node objects
     * @param links Vector of link objects
     */
    void initializeNetwork(
        const QVector<IntegrationNode *> &nodes,
        const QVector<IntegrationLink *> &links);

    /**
     * @brief Check if a node exists
     * @param nodeId Node identifier
     * @return True if node exists
     */
    bool nodeExists(int nodeId) const;

    /**
     * @brief Find shortest path between nodes
     * @param startNodeId Starting node ID
     * @param endNodeId Ending node ID
     * @return ShortestPathResult containing path details
     */
    ShortestPathResult findShortestPath(int startNodeId,
                                        int endNodeId);

    /**
     * @brief Get terminal nodes (those with no outgoing
     * edges)
     * @return Vector of terminal node IDs
     */
    QVector<int> getEndNodes() const;

    /**
     * @brief Get origin nodes (those with no incoming
     * edges)
     * @return Vector of origin node IDs
     */
    QVector<int> getStartNodes() const;

    /**
     * @brief Get all network nodes
     * @return Vector of nodes as objects pointer
     */
    QVector<IntegrationNode *> getNodes() const;

    /**
     * @brief Get all network links
     * @return Vector of links as JSON objects
     */
    QVector<IntegrationLink *> getLinks() const;

    /**
     * @brief Get the transportation graph
     * @return Const pointer to the transportation graph
     */
    const TransportationGraph<int> *getGraph() const;

    /**
     * @brief Get node with coordinates
     * @param nodeId Node identifier
     * @return Node as JSON object
     */
    IntegrationNode *getNode(int nodeId) const;

    /**
     * @brief Get link details
     * @param linkId Link identifier
     * @return Link as JSON object
     */
    IntegrationLink *getLink(int linkId) const;

    void setNetworkName(QString networkName);

    QString getNetworkName() const;

    /**
     * @brief Get multiple paths between nodes
     * @param startNodeId Starting node ID
     * @param endNodeId Ending node ID
     * @param maxPaths Maximum number of paths to find
     * @return List of paths as JSON objects
     */
    QList<ShortestPathResult>
    getMultiplePaths(int startNodeId, int endNodeId,
                     int maxPaths = 3);

    /**
     * @brief Adds a variable to the network
     * @param key Variable name
     * @param value Variable value
     */
    void setVariable(const QString  &key,
                     const QVariant &value) override;

    /**
     * @brief Gets a variable from the network
     * @param key Variable name
     * @return Variable value as QVariant
     */
    QVariant getVariable(const QString &key) const override;

    /**
     * @brief Gets all variables in the network
     * configuration
     * @return Map of all variables
     */
    QMap<QString, QVariant> getVariables() const override;

    /**
     * @brief Format as JSON
     * @return Network as JSON object
     */
    QJsonObject toJson() const;

signals:
    /**
     * @brief Signal emitted when network changes
     */
    void networkChanged();

    /**
     * @brief Signal emitted when nodes change
     */
    void nodesChanged();

    /**
     * @brief Signal emitted when links change
     */
    void linksChanged();

private:
    // Network name
    QString m_networkName;
    // Transportation graph for path-finding
    TransportationGraph<int> *m_graph = nullptr;

    // Node objects owned by this network
    QVector<IntegrationNode *> m_nodeObjects;

    // Link objects owned by this network
    QVector<IntegrationLink *> m_linkObjects;

    // Mutex for thread-safety
    mutable QMutex m_mutex;

    /**
     * @brief Get link IDs forming a path
     * @param pathNodes Vector of node IDs in the path
     * @return Vector of corresponding link IDs
     */
    QVector<int>
    getPathLinks(const QVector<int> &pathNodes) const;

    /**
     * @brief Get path length by links
     * @param linkIds Vector of link IDs
     * @return Total path length in km
     */
    double
    getPathLengthByLinks(const QVector<int> &linkIds) const;
};

/**
 * @class IntegrationSimulationConfig
 * @brief Configuration for truck simulation
 *
 * Manages simulation parameters, input/output paths,
 * and network configuration.
 */
class IntegrationSimulationConfig : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param parent Parent QObject
     */
    explicit IntegrationSimulationConfig(
        QObject *parent = nullptr);

    /**
     * @brief Destructor
     */
    ~IntegrationSimulationConfig();

    /**
     * @brief Initialize configuration
     * @param configDir Configuration directory
     * @param title Simulation title
     * @param simTime Simulation duration
     * @param inputFiles Map of input file paths
     * @param outputFiles Map of output file paths
     * @param inputFolder Input folder path
     * @param outputFolder Output folder path
     * @param additionalVariables Map of additional
     * variables
     * @return True if initialization successful
     */
    bool initialize(
        const QString &configDir, const QString &title,
        double                         simTime,
        const QMap<QString, QString>  &inputFiles,
        const QMap<QString, QString>  &outputFiles,
        const QString                 &inputFolder,
        const QString                 &outputFolder,
        const QMap<QString, QVariant> &additionalVariables);

    /**
     * @brief Get the network object
     * @return Shared network
     */
    IntegrationNetwork *getNetwork() const;

    /**
     * @brief Get simulation time
     * @return Simulation duration in seconds
     */
    double getSimTime() const
    {
        return m_simTime;
    }

    /**
     * @brief Get configuration directory
     * @return Path to configuration directory
     */
    QString getConfigDir() const
    {
        return m_configDir;
    }

    /**
     * @brief Get the original master configuration file path used to
     *        construct this integration config.
     */
    QString getSourceConfigPath() const
    {
        return m_sourceConfigPath;
    }

    /**
     * @brief Get input file path
     * @param key Input file key
     * @return Full path to input file
     */
    QString getInputFilePath(const QString &key) const;

    /**
     * @brief Get output file path
     * @param key Output file key
     * @return Full path to output file
     */
    QString getOutputFilePath(const QString &key) const;

    /**
     * @brief Format as JSON
     * @return Configuration as JSON object
     */
    QJsonObject toJson() const;

    /**
     * @brief Record the source master configuration file path used to
     *        build this config object.
     */
    void setSourceConfigPath(const QString &path)
    {
        m_sourceConfigPath = path;
    }

signals:
    /**
     * @brief Signal emitted when configuration changes
     */
    void configChanged();

private:
    // Configuration directory
    QString m_configDir;

    // Original config file path used to build this config.
    QString m_sourceConfigPath;

    // Simulation title
    QString m_title;

    // Simulation duration in seconds
    double m_simTime;

    // Input folder path
    QString m_inputFolder;

    // Output folder path
    QString m_outputFolder;

    // Map of input file keys to paths
    QMap<QString, QString> m_inputFiles;

    // Map of output file keys to paths
    QMap<QString, QString> m_outputFiles;

    // Configuration variables
    QMap<QString, QVariant> m_variables;

    // Shared network model
    IntegrationNetwork *m_network = nullptr;

    // Mutex for thread-safety
    mutable QMutex m_mutex;
};

/**
 * @class IntegrationSimulationConfigReader
 * @brief Reads simulation configuration files
 */
class IntegrationSimulationConfigReader : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param configFilePath Path to configuration file
     * @param parent Parent QObject
     */
    explicit IntegrationSimulationConfigReader(
        const QString &configFilePath,
        QObject       *parent = nullptr);

    /**
     * @brief Destructor
     */
    ~IntegrationSimulationConfigReader();

    /**
     * @brief Get parsed configuration
     * @return Shared configuration object
     */
    IntegrationSimulationConfig *getConfig() const
    {
        return m_config;
    }

    /**
     * @brief Read configuration from file
     * @param configFilePath Path to configuration file
     * @return Configuration as JSON object
     */
    static IntegrationSimulationConfig *
    readConfig(const QString &configFilePath);

private:
    // Parsed configuration
    IntegrationSimulationConfig *m_config;
};

} // namespace TruckClient
} // namespace Backend
} // namespace CargoNetSim
