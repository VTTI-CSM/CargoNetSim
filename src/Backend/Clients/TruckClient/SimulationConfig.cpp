/**
 * @file SimulationConfig.cpp
 * @brief Implements simulation configuration management
 * @author [Your Name]
 * @date 2025-03-22
 */

#include "SimulationConfig.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QTextStream>

#include "Backend/Commons/LogCategories.h"

namespace CargoNetSim
{
namespace Backend
{
namespace TruckClient
{

SimulationConfig::SimulationConfig(QObject *parent)
    : BaseObject(parent)
{
    qCDebug(lcClientTruck) << "SimulationConfig::SimulationConfig: default constructed";
}

bool SimulationConfig::loadFromFile(const QString &filePath)
{
    qCDebug(lcClientTruck) << "SimulationConfig::loadFromFile:" << filePath;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        qCWarning(lcClientTruck) << "SimulationConfig::loadFromFile:"
                                 << "cannot open file:" << filePath;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();
    qCDebug(lcClientTruck) << "SimulationConfig::loadFromFile: read"
                           << data.size() << "bytes";

    // Store file path
    m_configFilePath = filePath;

    // Parse JSON
    QJsonParseError error;
    QJsonDocument   doc =
        QJsonDocument::fromJson(data, &error);

    if (error.error != QJsonParseError::NoError)
    {
        qCWarning(lcClientTruck) << "SimulationConfig::loadFromFile:"
                                 << "JSON parse error:" << error.errorString()
                                 << "at offset" << error.offset;
        return false;
    }

    qCInfo(lcClientTruck) << "SimulationConfig::loadFromFile: loaded from" << filePath;
    return loadFromJson(doc.object());
}

bool SimulationConfig::saveToFile(const QString &filePath)
{
    QString path =
        filePath.isEmpty() ? m_configFilePath : filePath;
    qCDebug(lcClientTruck) << "SimulationConfig::saveToFile:" << path;

    if (path.isEmpty())
    {
        qCWarning(lcClientTruck) << "SimulationConfig::saveToFile: no file path specified";
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        qCWarning(lcClientTruck) << "SimulationConfig::saveToFile:"
                                 << "cannot open file for writing:" << path;
        return false;
    }

    QJsonDocument doc(toJson());
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    // Update file path if new
    if (!filePath.isEmpty())
    {
        m_configFilePath = filePath;
    }

    qCInfo(lcClientTruck) << "SimulationConfig::saveToFile: saved to" << path;
    return true;
}

bool SimulationConfig::loadFromJson(
    const QJsonObject &config)
{
    qCDebug(lcClientTruck) << "SimulationConfig::loadFromJson:"
                           << "keys=" << config.keys();
    m_config = config;
    emit configurationChanged();
    return true;
}

QJsonObject SimulationConfig::toJson() const
{
    qCDebug(lcClientTruck) << "SimulationConfig::toJson:"
                           << "keys=" << m_config.keys();
    return m_config;
}

bool SimulationConfig::validate(QStringList *errors) const
{
    qCDebug(lcClientTruck) << "SimulationConfig::validate: starting validation";
    bool        valid = true;
    QStringList errorList;

    // Check required top-level properties
    if (!m_config.contains("simulation"))
    {
        errorList << "Missing 'simulation' section";
        valid = false;
    }

    if (!m_config.contains("networks"))
    {
        errorList << "Missing 'networks' section";
        valid = false;
    }

    // Check simulation section
    if (m_config.contains("simulation"))
    {
        QJsonObject sim = m_config["simulation"].toObject();

        if (!sim.contains("duration"))
        {
            errorList << "Missing 'simulation.duration'";
            valid = false;
        }

        if (!sim.contains("time_step"))
        {
            errorList << "Missing 'simulation.time_step'";
            valid = false;
        }
    }

    // Check networks section
    if (m_config.contains("networks"))
    {
        QJsonArray networks =
            m_config["networks"].toArray();

        if (networks.isEmpty())
        {
            errorList << "No networks defined";
            valid = false;
        }

        for (int i = 0; i < networks.size(); i++)
        {
            QJsonObject network = networks[i].toObject();

            if (!network.contains("name"))
            {
                errorList
                    << QString(
                           "Network at index %1 missing "
                           "name")
                           .arg(i);
                valid = false;
            }

            if (!network.contains("master_file"))
            {
                errorList
                    << QString("Network '%1' missing "
                               "master_file")
                           .arg(network["name"].toString());
                valid = false;
            }
        }
    }

    // Set errors if requested
    if (errors)
    {
        *errors = errorList;
    }

    if (valid)
    {
        qCInfo(lcClientTruck) << "SimulationConfig::validate: configuration is valid";
    }
    else
    {
        qCWarning(lcClientTruck) << "SimulationConfig::validate: configuration invalid,"
                                 << errorList.size() << "errors:" << errorList;
    }

    return valid;
}

QVariant SimulationConfig::getValue(
    const QString &key, const QVariant &defaultValue) const
{
    qCDebug(lcClientTruck) << "SimulationConfig::getValue: key=" << key;

    if (!m_config.contains(key))
    {
        qCDebug(lcClientTruck) << "SimulationConfig::getValue: key not found, using default";
        return defaultValue;
    }

    QJsonValue value = m_config[key];

    switch (value.type())
    {
    case QJsonValue::Bool:
        return value.toBool();
    case QJsonValue::Double:
        return value.toDouble();
    case QJsonValue::String:
        return value.toString();
    case QJsonValue::Array:
        return value.toArray().toVariantList();
    case QJsonValue::Object:
        return value.toObject().toVariantMap();
    default:
        return defaultValue;
    }
}

void SimulationConfig::setValue(const QString  &key,
                                const QVariant &value)
{
    qCDebug(lcClientTruck) << "SimulationConfig::setValue: key=" << key
                           << "type=" << value.typeName();

    QJsonValue jsonValue;

    switch (value.typeId())
    {
    case QMetaType::Bool:
        jsonValue = value.toBool();
        break;
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
    case QMetaType::Double:
        jsonValue = value.toDouble();
        break;
    case QMetaType::QString:
        jsonValue = value.toString();
        break;
    case QMetaType::QVariantList:
    case QMetaType::QStringList: {
        QJsonArray   array;
        QVariantList list = value.toList();
        for (const QVariant &item : list)
        {
            array.append(QJsonValue::fromVariant(item));
        }
        jsonValue = array;
    }
    break;
    case QMetaType::QVariantMap: {
        QJsonObject obj;
        QVariantMap map = value.toMap();
        for (auto it = map.begin(); it != map.end(); ++it)
        {
            obj[it.key()] =
                QJsonValue::fromVariant(it.value());
        }
        jsonValue = obj;
    }
    break;
    default:
        qCWarning(lcClientTruck) << "SimulationConfig::setValue:"
                                 << "unsupported type for key:" << key;
        return;
    }

    m_config[key] = jsonValue;
    emit configurationChanged();
}

QVariant SimulationConfig::getNestedValue(
    const QString &path, const QVariant &defaultValue) const
{
    qCDebug(lcClientTruck) << "SimulationConfig::getNestedValue: path=" << path;

    // Split path by separator
    QStringList segments = path.split('/');

    // Start from root
    QJsonObject current = m_config;

    // Navigate path except last segment
    for (int i = 0; i < segments.size() - 1; i++)
    {
        QString segment = segments[i];

        if (!current.contains(segment))
        {
            qCWarning(lcClientTruck) << "SimulationConfig::getNestedValue:"
                                     << "segment not found:" << segment
                                     << "in path:" << path << "using default";
            return defaultValue;
        }

        if (!current[segment].isObject())
        {
            qCWarning(lcClientTruck) << "SimulationConfig::getNestedValue:"
                                     << "segment is not an object:" << segment;
            return defaultValue;
        }

        current = current[segment].toObject();
    }

    // Get final value
    QString lastSegment = segments.last();

    if (!current.contains(lastSegment))
    {
        qCWarning(lcClientTruck) << "SimulationConfig::getNestedValue:"
                                 << "key not found:" << lastSegment
                                 << "in path:" << path << "using default";
        return defaultValue;
    }

    return QJsonValue(current[lastSegment]).toVariant();
}

void SimulationConfig::setNestedValue(const QString  &path,
                                      const QVariant &value)
{
    qCDebug(lcClientTruck) << "SimulationConfig::setNestedValue: path=" << path;
    QStringList segments = path.split('/');

    // Build path in config
    QJsonObject *current = &m_config;

    // Create nested objects as needed
    for (int i = 0; i < segments.size() - 1; i++)
    {
        QString segment = segments[i];

        if (!current->contains(segment)
            || !(*current)[segment].isObject())
        {
            (*current)[segment] = QJsonObject();
        }

        auto result = (*current)[segment].toObject();

        current = const_cast<QJsonObject *>(&result);
    }

    // Set final value
    QString lastSegment = segments.last();
    (*current)[lastSegment] =
        QJsonValue::fromVariant(value);

    emit configurationChanged();
}

QString
SimulationConfig::getFilePath(const QString &key,
                              const QString &basePath) const
{
    qCDebug(lcClientTruck) << "SimulationConfig::getFilePath: key=" << key
                           << "basePath=" << basePath;

    QString path = getValue(key).toString();

    if (path.isEmpty())
    {
        qCWarning(lcClientTruck) << "SimulationConfig::getFilePath: empty path for key:" << key;
        return QString();
    }

    // Check if path is absolute
    if (QFileInfo(path).isAbsolute())
    {
        qCDebug(lcClientTruck) << "SimulationConfig::getFilePath: absolute path:" << path;
        return path;
    }

    // Use provided base path or config file path
    QString base = basePath;
    if (base.isEmpty() && !m_configFilePath.isEmpty())
    {
        base = QFileInfo(m_configFilePath).absolutePath();
    }

    if (base.isEmpty())
    {
        qCDebug(lcClientTruck) << "SimulationConfig::getFilePath: no base, returning relative:" << path;
        return path;
    }

    QString result = QDir(base).filePath(path);
    qCDebug(lcClientTruck) << "SimulationConfig::getFilePath: resolved:" << result;
    return result;
}

void SimulationConfig::merge(const SimulationConfig &other,
                             bool overwrite)
{
    qCDebug(lcClientTruck) << "SimulationConfig::merge: overwrite=" << overwrite;

    QJsonObject otherConfig = other.toJson();
    int merged = 0;

    for (auto it = otherConfig.begin();
         it != otherConfig.end(); ++it)
    {
        QString key = it.key();

        // Skip if exists and not overwriting
        if (m_config.contains(key) && !overwrite)
        {
            qCDebug(lcClientTruck) << "SimulationConfig::merge: skipping existing key:" << key;
            continue;
        }

        m_config[key] = it.value();
        ++merged;
    }

    qCInfo(lcClientTruck) << "SimulationConfig::merge: merged" << merged << "keys";
    emit configurationChanged();
}

QJsonValue
SimulationConfig::getJsonValue(const QString &path) const
{
    qCDebug(lcClientTruck) << "SimulationConfig::getJsonValue: path=" << path;

    QStringList segments = path.split('/');

    // Start from root
    QJsonObject current = m_config;

    // Navigate path except last segment
    for (int i = 0; i < segments.size() - 1; i++)
    {
        QString segment = segments[i];

        if (!current.contains(segment))
        {
            qCDebug(lcClientTruck) << "SimulationConfig::getJsonValue:"
                                   << "segment not found:" << segment;
            return QJsonValue();
        }

        if (!current[segment].isObject())
        {
            qCWarning(lcClientTruck) << "SimulationConfig::getJsonValue:"
                                     << "segment is not an object:" << segment;
            return QJsonValue();
        }

        current = current[segment].toObject();
    }

    // Get final value
    QString lastSegment = segments.last();

    if (!current.contains(lastSegment))
    {
        qCDebug(lcClientTruck) << "SimulationConfig::getJsonValue:"
                               << "final segment not found:" << lastSegment;
        return QJsonValue();
    }

    return current[lastSegment];
}

void SimulationConfig::setJsonValue(const QString    &path,
                                    const QJsonValue &value)
{
    qCDebug(lcClientTruck) << "SimulationConfig::setJsonValue: path=" << path;
    QStringList segments = path.split('/');

    // Build path in config
    QJsonObject *current = &m_config;

    // Create nested objects as needed
    for (int i = 0; i < segments.size() - 1; i++)
    {
        QString segment = segments[i];

        if (!current->contains(segment)
            || !(*current)[segment].isObject())
        {
            (*current)[segment] = QJsonObject();
        }

        auto result = (*current)[segment].toObject();
        current     = const_cast<QJsonObject *>(&result);
    }

    // Set final value
    QString lastSegment     = segments.last();
    (*current)[lastSegment] = value;

    emit configurationChanged();
}

} // namespace TruckClient
} // namespace Backend
} // namespace CargoNetSim
