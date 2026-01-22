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

namespace CargoNetSim
{
namespace Backend
{
namespace TruckClient
{

SimulationConfig::SimulationConfig(QObject *parent)
    : BaseObject(parent)
{
}

bool SimulationConfig::loadFromFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    // Store file path
    m_configFilePath = filePath;

    // Parse JSON
    QJsonParseError error;
    QJsonDocument   doc =
        QJsonDocument::fromJson(data, &error);

    if (error.error != QJsonParseError::NoError)
    {
        return false;
    }

    return loadFromJson(doc.object());
}

bool SimulationConfig::saveToFile(const QString &filePath)
{
    QString path =
        filePath.isEmpty() ? m_configFilePath : filePath;
    if (path.isEmpty())
    {
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
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

    return true;
}

bool SimulationConfig::loadFromJson(
    const QJsonObject &config)
{
    m_config = config;
    emit configurationChanged();
    return true;
}

QJsonObject SimulationConfig::toJson() const
{
    return m_config;
}

bool SimulationConfig::validate(QStringList *errors) const
{
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

    return valid;
}

QVariant SimulationConfig::getValue(
    const QString &key, const QVariant &defaultValue) const
{
    if (!m_config.contains(key))
    {
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
        return;
    }

    m_config[key] = jsonValue;
    emit configurationChanged();
}

QVariant SimulationConfig::getNestedValue(
    const QString &path, const QVariant &defaultValue) const
{
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
            return defaultValue;
        }

        if (!current[segment].isObject())
        {
            return defaultValue;
        }

        current = current[segment].toObject();
    }

    // Get final value
    QString lastSegment = segments.last();

    if (!current.contains(lastSegment))
    {
        return defaultValue;
    }

    return QJsonValue(current[lastSegment]).toVariant();
}

void SimulationConfig::setNestedValue(const QString  &path,
                                      const QVariant &value)
{
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
    QString path = getValue(key).toString();

    if (path.isEmpty())
    {
        return QString();
    }

    // Check if path is absolute
    if (QFileInfo(path).isAbsolute())
    {
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
        return path;
    }

    return QDir(base).filePath(path);
}

void SimulationConfig::merge(const SimulationConfig &other,
                             bool overwrite)
{
    QJsonObject otherConfig = other.toJson();

    for (auto it = otherConfig.begin();
         it != otherConfig.end(); ++it)
    {
        QString key = it.key();

        // Skip if exists and not overwriting
        if (m_config.contains(key) && !overwrite)
        {
            continue;
        }

        m_config[key] = it.value();
    }

    emit configurationChanged();
}

QJsonValue
SimulationConfig::getJsonValue(const QString &path) const
{
    QStringList segments = path.split('/');

    // Start from root
    QJsonObject current = m_config;

    // Navigate path except last segment
    for (int i = 0; i < segments.size() - 1; i++)
    {
        QString segment = segments[i];

        if (!current.contains(segment))
        {
            return QJsonValue();
        }

        if (!current[segment].isObject())
        {
            return QJsonValue();
        }

        current = current[segment].toObject();
    }

    // Get final value
    QString lastSegment = segments.last();

    if (!current.contains(lastSegment))
    {
        return QJsonValue();
    }

    return current[lastSegment];
}

void SimulationConfig::setJsonValue(const QString    &path,
                                    const QJsonValue &value)
{
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
