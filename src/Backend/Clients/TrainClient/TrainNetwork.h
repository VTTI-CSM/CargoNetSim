/**
 * @file TrainNetwork.h
 * @brief Train network simulation classes for CargoNetSim
 * @author Ahmed Aredah
 * @date 2025-03-19
 * @copyright Copyright (c) 2025
 */

#pragma once

#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QPair>
#include <QQueue>
#include <QString>
#include <QTextStream>
#include <QVariant>
#include <QVector>

#include "Backend/Commons/DirectedGraph.h"
#include "Backend/Commons/ShortestPathResult.h"
#include "Backend/Commons/Units.h"
#include "Backend/Models/BaseNetwork.h"
#include "Backend/Models/BaseObject.h"

namespace CargoNetSim
{
namespace Backend
{
namespace TrainClient
{

/**
 * @class NeTrainSimNode
 * @brief Represents a node in the train simulation network
 *
 * The NeTrainSimNode class models a network node with
 * spatial coordinates and other properties such as dwell
 * time.
 *
 * This class must be registered with Qt's meta-object
 * system using
 * Q_DECLARE_METATYPE(CargoNetSim::Backend::NeTrainSimNode)
 * after class definition.
 */
class NeTrainSimNode : public BaseObject
{
    Q_OBJECT

public:
    /**
     * @brief Default constructor
     * @param parent The parent QObject
     */
    NeTrainSimNode(QObject *parent = nullptr);

    /**
     * @brief Parameterized constructor
     * @param simulatorId Internal identifier for the
     * simulator
     * @param userId User-defined identifier
     * @param x X-coordinate on the network
     * @param y Y-coordinate on the network
     * @param description Text description of the node
     * @param xScale Scaling factor for X-coordinate
     * @param yScale Scaling factor for Y-coordinate
     * @param isTerminal Whether node is a terminal station
     * @param dwellTime Time spent at this node in seconds
     * @param parent The parent QObject
     */
    NeTrainSimNode(int simulatorId, int userId, float x,
                   float y, const QString &description,
                   float xScale, float yScale,
                   bool     isTerminal = false,
                   float    dwellTime  = 0.0f,
                   QObject *parent     = nullptr);

    /**
     * @brief Constructor from JSON data
     * @param json JSON object containing node data
     * @param parent The parent QObject
     */
    NeTrainSimNode(const QJsonObject &json,
                   QObject           *parent = nullptr);

    /**
     * @brief Converts the node to a JSON object
     * @return QJsonObject representation of the node
     */
    QJsonObject toDict() const;

    /**
     * @brief Creates a node from JSON data
     * @param data JSON object containing node data
     * @param parent The parent QObject
     * @return Pointer to the new node
     */
    static NeTrainSimNode *
    fromDict(const QJsonObject &data,
             QObject           *parent = nullptr);

    /**
     * @brief Gets the simulator ID
     * @return Simulator identifier
     */
    int getSimulatorId() const
    {
        return m_simulatorId;
    }

    /**
     * @brief Gets the user ID
     * @return User-defined identifier
     */
    int getUserId() const
    {
        return m_userId;
    }

    /**
     * @brief Gets the X coordinate
     * @return X coordinate value
     */
    float getX() const
    {
        return m_x;
    }

    /**
     * @brief Gets the Y coordinate
     * @return Y coordinate value
     */
    float getY() const
    {
        return m_y;
    }

    /**
     * @brief Gets the node getDescription
     * @return Description text
     */
    QString getDescription() const
    {
        return m_description;
    }

    /**
     * @brief Gets the X scaling factor
     * @return X scale value
     */
    float getXScale() const
    {
        return m_xScale;
    }

    /**
     * @brief Gets the Y scaling factor
     * @return Y scale value
     */
    float getYScale() const
    {
        return m_yScale;
    }

    /**
     * @brief Gets the terminal status
     * @return True if node is a terminal
     */
    bool isTerminal() const
    {
        return m_isTerminal;
    }

    /**
     * @brief Gets the dwell time
     * @return Dwell time in seconds
     */
    float getDwellTime() const
    {
        return m_dwellTime;
    }

    Units::TimeSeconds dwellTimeUnits() const
    {
        return Units::seconds(m_dwellTime);
    }

    /**
     * @brief Sets the simulator ID
     * @param simulatorId Simulator identifier
     */
    void setSimulatorId(int simulatorId);

    /**
     * @brief Sets the user ID
     * @param userId User-defined identifier
     */
    void setUserId(int userId);

    /**
     * @brief Sets the X coordinate
     * @param x X coordinate value
     */
    void setX(float x);

    /**
     * @brief Sets the Y coordinate
     * @param y Y coordinate value
     */
    void setY(float y);

    /**
     * @brief Sets the node description
     * @param description Description text
     */
    void setDescription(const QString &description);

    /**
     * @brief Sets the X scaling factor
     * @param xScale X scale value
     */
    void setXScale(float xScale);

    /**
     * @brief Sets the Y scaling factor
     * @param yScale Y scale value
     */
    void setYScale(float yScale);

    /**
     * @brief Sets the terminal status
     * @param isTerminal True if node is a terminal
     */
    void setIsTerminal(bool isTerminal);

    /**
     * @brief Sets the dwell time
     * @param dwellTime Dwell time in seconds
     */
    void setDwellTime(float dwellTime);

    void setDwellTimeUnits(Units::TimeSeconds dwellTime)
    {
        setDwellTime(static_cast<float>(dwellTime.value()));
    }

signals:
    /**
     * @brief Signal emitted when any node property changes
     */
    void nodeChanged();

private:
    int   m_simulatorId; ///< Internal simulator identifier
    int   m_userId;      ///< User-defined identifier
    float m_x;           ///< X-coordinate on the network
    float m_y;           ///< Y-coordinate on the network
    QString m_description; ///< Text description of the node
    float   m_xScale; ///< Scaling factor for X-coordinate
    float   m_yScale; ///< Scaling factor for Y-coordinate
    bool    m_isTerminal; ///< Whether node is a terminal
                          ///< station
    float m_dwellTime; ///< Time spent at this node in seconds
};

/**
 * @class NeTrainSimLink
 * @brief Represents a link between nodes in train network
 *
 * The NeTrainSimLink class models a connection between two
 * nodes with properties such as length, speed limits,
 * grade, etc.
 *
 * This class must be registered with Qt's meta-object
 * system using
 * Q_DECLARE_METATYPE(CargoNetSim::Backend::NeTrainSimLink)
 * after class definition.
 */
class NeTrainSimLink : public BaseObject
{
    Q_OBJECT

public:
    /**
     * @brief Default constructor
     * @param parent The parent QObject
     */
    NeTrainSimLink(QObject *parent = nullptr);

    /**
     * @brief Parameterized constructor
     * @param simulatorId Internal identifier for the
     * simulator
     * @param userId User-defined identifier
     * @param fromNode Source node of the link
     * @param toNode Destination node of the link
     * @param length Length of the link in meters
     * @param maxSpeed Maximum speed allowed on link in m/s
     * @param signalId Identifier for the signal system
     * @param signalsAtNodes Signal configuration at nodes
     * @param grade Grade percentage (positive = uphill)
     * @param curvature Curvature measure in degrees
     * @param numDirections Number of possible travel
     * directions
     * @param speedVariationFactor Factor affecting speed
     * variation
     * @param hasCatenary Whether link has overhead power
     * @param region Geographic region identifier
     * @param lengthScale Scaling factor for link length
     * @param speedScale Scaling factor for speed limits
     * @param parent The parent QObject
     */
    NeTrainSimLink(int simulatorId, int userId,
                   NeTrainSimNode *fromNode,
                   NeTrainSimNode *toNode, float length,
                   float maxSpeed, int signalId,
                   const QString &signalsAtNodes,
                   float grade, float curvature,
                   int   numDirections,
                   float speedVariationFactor,
                   bool hasCatenary, const QString &region,
                   float lengthScale, float speedScale,
                   QObject *parent = nullptr);

    /**
     * @brief Constructor from JSON data
     * @param json JSON object containing link data
     * @param parent The parent QObject
     */
    NeTrainSimLink(const QJsonObject &json,
                   QObject           *parent = nullptr);

    /**
     * @brief Converts the link to a JSON object
     * @return QJsonObject representation of the link
     */
    QJsonObject toDict() const;

    /**
     * @brief Creates a link from JSON data
     * @param data JSON object containing link data
     * @param parent The parent QObject
     * @return Pointer to the new link
     */
    static NeTrainSimLink *
    fromDict(const QJsonObject &data,
             QObject           *parent = nullptr);

    /**
     * @brief Gets the simulator ID
     * @return Simulator identifier
     */
    int getSimulatorId() const
    {
        return m_simulatorId;
    }

    /**
     * @brief Gets the user ID
     * @return User-defined identifier
     */
    int getUserId() const
    {
        return m_userId;
    }

    /**
     * @brief Gets the source node
     * @return Pointer to the source node
     */
    NeTrainSimNode *getFromNode() const
    {
        return m_fromNode;
    }

    /**
     * @brief Gets the destination node
     * @return Pointer to the destination node
     */
    NeTrainSimNode *getToNode() const
    {
        return m_toNode;
    }

    /**
     * @brief Gets the link getLength
     * @return Length in meters
     */
    float getLength() const
    {
        return m_length;
    }

    Units::LengthMeters lengthUnits() const
    {
        return Units::meters(m_length);
    }

    Units::LengthMeters scaledLengthUnits() const
    {
        return Units::meters(m_length * m_lengthScale);
    }

    /**
     * @brief Gets the maximum speed
     * @return Speed in m/s
     */
    float getMaxSpeed() const
    {
        return m_maxSpeed;
    }

    Units::SpeedMetersPerSecond maxSpeedUnits() const
    {
        return Units::metersPerSecond(m_maxSpeed);
    }

    Units::SpeedMetersPerSecond scaledMaxSpeedUnits() const
    {
        return Units::metersPerSecond(m_maxSpeed * m_speedScale);
    }

    /**
     * @brief Gets the signal ID
     * @return Signal system identifier
     */
    int getSignalId() const
    {
        return m_signalId;
    }

    /**
     * @brief Gets the signals at nodes configuration
     * @return Signal configuration string
     */
    QString getSignalsAtNodes() const
    {
        return m_signalsAtNodes;
    }

    /**
     * @brief Gets the getGrade percentage
     * @return Grade value (positive = uphill)
     */
    float getGrade() const
    {
        return m_grade;
    }

    /**
     * @brief Gets the getCurvature
     * @return Curvature in degrees
     */
    float getCurvature() const
    {
        return m_curvature;
    }

    /**
     * @brief Gets the number of directions
     * @return Number of travel directions
     */
    int getNumDirections() const
    {
        return m_numDirections;
    }

    /**
     * @brief Gets the speed variation factor
     * @return Speed variation factor
     */
    float getSpeedVariationFactor() const
    {
        return m_speedVariationFactor;
    }

    /**
     * @brief Gets the catenary status
     * @return True if link has overhead power
     */
    bool hasCatenary() const
    {
        return m_hasCatenary;
    }

    /**
     * @brief Gets the getRegion
     * @return Region identifier string
     */
    QString getRegion() const
    {
        return m_region;
    }

    /**
     * @brief Gets the length scale
     * @return Length scaling factor
     */
    float getLengthScale() const
    {
        return m_lengthScale;
    }

    /**
     * @brief Gets the speed scale
     * @return Speed scaling factor
     */
    float getSpeedScale() const
    {
        return m_speedScale;
    }

    /**
     * @brief Sets the simulator ID
     * @param simulatorId Simulator identifier
     */
    void setSimulatorId(int simulatorId);

    /**
     * @brief Sets the user ID
     * @param userId User-defined identifier
     */
    void setUserId(int userId);

    /**
     * @brief Sets the source node
     * @param fromNode Pointer to the source node
     */
    void setFromNode(NeTrainSimNode *fromNode);

    /**
     * @brief Sets the destination node
     * @param toNode Pointer to the destination node
     */
    void setToNode(NeTrainSimNode *toNode);

    /**
     * @brief Sets the link length
     * @param length Length in meters
     */
    void setLength(float length);

    void setLengthUnits(Units::LengthMeters length)
    {
        setLength(static_cast<float>(length.value()));
    }

    /**
     * @brief Sets the maximum speed
     * @param maxSpeed Speed in m/s
     */
    void setMaxSpeed(float maxSpeed);

    void setMaxSpeedUnits(Units::SpeedMetersPerSecond maxSpeed)
    {
        setMaxSpeed(static_cast<float>(maxSpeed.value()));
    }

    /**
     * @brief Sets the signal ID
     * @param signalId Signal system identifier
     */
    void setSignalId(int signalId);

    /**
     * @brief Sets the signals at nodes configuration
     * @param signalsAtNodes Signal configuration string
     */
    void setSignalsAtNodes(const QString &signalsAtNodes);

    /**
     * @brief Sets the grade percentage
     * @param grade Grade value (positive = uphill)
     */
    void setGrade(float grade);

    /**
     * @brief Sets the curvature
     * @param curvature Curvature in degrees
     */
    void setCurvature(float curvature);

    /**
     * @brief Sets the number of directions
     * @param numDirections Number of travel directions
     */
    void setNumDirections(int numDirections);

    /**
     * @brief Sets the speed variation factor
     * @param speedVariationFactor Speed variation factor
     */
    void
    setSpeedVariationFactor(float speedVariationFactor);

    /**
     * @brief Sets the catenary status
     * @param hasCatenary True if link has overhead power
     */
    void setHasCatenary(bool hasCatenary);

    /**
     * @brief Sets the region
     * @param region Region identifier string
     */
    void setRegion(const QString &region);

    /**
     * @brief Sets the length scale
     * @param lengthScale Length scaling factor
     */
    void setLengthScale(float lengthScale);

    /**
     * @brief Sets the speed scale
     * @param speedScale Speed scaling factor
     */
    void setSpeedScale(float speedScale);

signals:
    /**
     * @brief Signal emitted when any link property changes
     */
    void linkChanged();

private:
    int m_simulatorId; ///< Internal simulator identifier
    int m_userId;      ///< User-defined identifier
    NeTrainSimNode *m_fromNode; ///< Source node of the link
    NeTrainSimNode
         *m_toNode;   ///< Destination node of the link
    float m_length;   ///< Length of the link in meters
    float m_maxSpeed; ///< Maximum speed allowed in m/s
    int   m_signalId; ///< Identifier for the signal system
    QString
        m_signalsAtNodes; ///< Signal configuration at nodes
    float m_grade;        ///< Grade percentage
    float m_curvature;    ///< Curvature measure in degrees
    int m_numDirections;  ///< Number of possible directions
    float
         m_speedVariationFactor; ///< Speed variation factor
    bool m_hasCatenary; ///< Whether link has overhead power
    QString m_region;   ///< Geographic region identifier
    float m_lengthScale; ///< Scaling factor for link length
    float m_speedScale; ///< Scaling factor for speed limits
};

/**
 * @class NeTrainSimNodeDataReader
 * @brief Utility class for reading node data from files
 *
 * The NeTrainSimNodeDataReader provides static methods to
 * parse and load node data from text files.
 *
 * This class must be registered with Qt's meta-object
 * system using
 * Q_DECLARE_METATYPE(CargoNetSim::Backend::NeTrainSimNodeDataReader)
 * after class definition.
 */
class NeTrainSimNodeDataReader : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Reads node data from a file
     * @param filename Path to the nodes data file
     * @return Vector of maps containing node data records
     */
    static QVector<QMap<QString, QString>>
    readNodesFile(const QString &filename);
};

/**
 * @class NeTrainSimLinkDataReader
 * @brief Utility class for reading link data from files
 *
 * The NeTrainSimLinkDataReader provides static methods to
 * parse and load link data from text files.
 *
 * This class must be registered with Qt's meta-object
 * system using
 * Q_DECLARE_METATYPE(CargoNetSim::Backend::NeTrainSimLinkDataReader)
 * after class definition.
 */
class NeTrainSimLinkDataReader : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Reads link data from a file
     * @param filename Path to the links data file
     * @return Vector of maps containing link data records
     */
    static QVector<QMap<QString, QString>>
    readLinksFile(const QString &filename);
};

/**
 * @class NeTrainSimNetwork
 * @brief Base class for train simulation network
 *
 * The NeTrainSimNetwork manages a network of nodes and
 * links, providing path-finding capabilities and network
 * operations.
 */
class NeTrainSimNetwork : public BaseNetwork
{
    Q_OBJECT

public:
    /**
     * @brief Default constructor
     * @param parent The parent QObject
     */
    NeTrainSimNetwork(QObject *parent = nullptr);

    /**
     * @brief Destructor
     *
     * Properly cleans up all network resources.
     */
    ~NeTrainSimNetwork();

    /**
     * @brief Adds a variable to the network configuration
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
     * @brief Loads a network from node and link files
     * @param nodesFile Path to the nodes data file
     * @param linksFile Path to the links data file
     */
    void loadNetwork(const QString &nodesFile,
                     const QString &linksFile);

    /**
     * @brief Gets all nodes in the network as JSON objects
     * @return QJsonArray of node data as JSON objects
     */
    QJsonArray getNodesAsJson() const;

    /**
     * @brief Gets all links in the network as JSON objects
     * @return QJsonArray of link data as JSON objects
     */
    QJsonArray getLinksAsJson() const;

    /**
     * @brief Gets all nodes in the network
     * @return Vector of node pointers
     */
    QVector<NeTrainSimNode *> getNodes() const;

    /**
     * @brief Gets all links in the network
     * @return Vector of link pointers
     */
    QVector<NeTrainSimLink *> getLinks() const;

    /**
     * @brief Gets a node by its user ID
     * @param id User-defined node identifier
     * @return Pointer to the node or nullptr if not found
     */
    const NeTrainSimNode *getNodeByID(int id) const;

    /**
     * @brief Sets the network name
     * @param networkName Network name
     */
    void setNetworkName(QString networkName);

    /**
     * @brief Gets the network name
     * @return Network name
     */
    QString getNetworkName() const;

    /**
     * @brief Gets link information for a path
     * @param path Vector of node IDs representing a path
     * @return Pair of vectors containing link IDs and
     * lengths
     */
    QPair<QVector<int>, QVector<float>>
    getPathLinks(const QVector<int> &path) const;

    /**
     * @brief Finds the shortest path between two nodes
     * @param startNodeId Starting node ID
     * @param endNodeId Ending node ID
     * @param optimizeFor Optimization criteria ("distance"
     * or "time")
     * @return ShortestPathResult struct with path details
     */
    ShortestPathResult findShortestPath(
        int startNodeId, int endNodeId,
        const QString &optimizeFor = "distance");

    /**
     * @brief Converts all nodes to a JSON object
     * @return JSON object containing all nodes
     */
    QJsonObject nodesToJson() const;

    /**
     * @brief Converts all links to a JSON object
     * @return JSON object containing all links
     */
    QJsonObject linksToJson() const;

    /**
     * @brief Sets nodes and links from JSON data
     * @param nodes Vector of node data as JSON objects
     * @param links Vector of link data as JSON objects
     */
    void setNodesAndLinksFromJson(
        const QVector<QJsonObject> &nodes,
        const QVector<QJsonObject> &links);

    /**
     * @brief Initializes the directed graph from nodes and
     * links
     */
    void initializeGraph();

signals:
    /**
     * @brief Signal emitted when the network changes
     */
    void networkChanged();

    /**
     * @brief Signal emitted when nodes are modified
     */
    void nodesChanged();

    /**
     * @brief Signal emitted when links are modified
     */
    void linksChanged();

private:
    /**
     * @brief Generates node objects from data records
     * @param nodeRecords Vector of node data records
     * @return Vector of node objects
     */
    QVector<NeTrainSimNode *> generateNodes(
        const QVector<QMap<QString, QString>> &nodeRecords);

    /**
     * @brief Generates link objects from data records
     * @param linkRecords Vector of link data records
     * @return Vector of link objects
     */
    QVector<NeTrainSimLink *> generateLinks(
        const QVector<QMap<QString, QString>> &linkRecords);

    /**
     * @brief Finds a node by its user ID
     * @param userId User-defined node identifier
     * @return Pointer to the node or nullptr if not found
     */
    NeTrainSimNode *getNodeByUserId(int userId) const;

    /**
     * @brief Builds the directed graph from nodes and links
     */
    void buildGraph();

    QString m_networkName;             ///< Network name
    QVector<NeTrainSimNode *> m_nodes; ///< Node objects
    QVector<NeTrainSimLink *> m_links; ///< Link objects
    DirectedGraph<int>
        *m_graph; ///< Directed graph of network

    mutable QMutex
        m_mutex; ///< Thread synchronization mutex
};

} // namespace TrainClient
} // namespace Backend
} // namespace CargoNetSim

// Register custom types with Qt's meta-object system
Q_DECLARE_METATYPE(
    CargoNetSim::Backend::TrainClient::NeTrainSimNode)
Q_DECLARE_METATYPE(
    CargoNetSim::Backend::TrainClient::NeTrainSimNode *)
Q_DECLARE_METATYPE(
    CargoNetSim::Backend::TrainClient::NeTrainSimLink)
Q_DECLARE_METATYPE(
    CargoNetSim::Backend::TrainClient::NeTrainSimLink *)
Q_DECLARE_METATYPE(CargoNetSim::Backend::TrainClient::
                       NeTrainSimNodeDataReader)
Q_DECLARE_METATYPE(CargoNetSim::Backend::TrainClient::
                       NeTrainSimNodeDataReader *)
Q_DECLARE_METATYPE(CargoNetSim::Backend::TrainClient::
                       NeTrainSimLinkDataReader)
Q_DECLARE_METATYPE(CargoNetSim::Backend::TrainClient::
                       NeTrainSimLinkDataReader *)
Q_DECLARE_METATYPE(
    CargoNetSim::Backend::TrainClient::NeTrainSimNetwork)
Q_DECLARE_METATYPE(
    CargoNetSim::Backend::TrainClient::NeTrainSimNetwork *)
