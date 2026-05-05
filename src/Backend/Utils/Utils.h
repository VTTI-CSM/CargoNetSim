#include "Backend/Commons/LogCategories.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QString>

namespace CargoNetSim
{
namespace Backend
{
namespace Utils
{

/**
 * @brief Find the path to a config file by filename
 * @param filename The config filename (e.g., "config.xml",
 *                 "rabbitmq.xml")
 * @return The full path to the config file
 */
static QString findConfigFilePath(const QString &filename)
{
    QString configPath = "config/" + filename;

    // First, try to find the config file in the directory
    // beside the executable
    QDir execDir(QCoreApplication::applicationDirPath());
    if (execDir.exists(configPath))
    {
        return execDir.filePath(configPath);
    }

    // Next, try one directory up (in case the executable is
    // in a bin/ subdirectory)
    QDir parentDir = execDir;
    if (parentDir.cdUp() && parentDir.exists(configPath))
    {
        return parentDir.filePath(configPath);
    }

    // For development: try to find it relative to the
    // source directory
    QDir repoDir(QCoreApplication::applicationDirPath());
    while (!repoDir.exists(configPath) && repoDir.cdUp())
    {
        // Keep going up until we find the config directory
        // or hit the root
    }

    if (repoDir.exists(configPath))
    {
        return repoDir.filePath(configPath);
    }

    // Fallback - create config in user's config location
    QString fallbackPath =
        QStandardPaths::writableLocation(
            QStandardPaths::AppConfigLocation)
        + "/" + filename;

    // Ensure the directory exists
    QDir configDir = QFileInfo(fallbackPath).dir();
    if (!configDir.exists())
    {
        configDir.mkpath(".");
    }

    qCWarning(lcConfig)
        << "Config file not found, will create new one at:"
        << fallbackPath;
    return fallbackPath;
}

/**
 * @brief Find the path to the main config file (config.xml)
 * @return The full path to config.xml
 */
static QString findConfigFilePath()
{
    return findConfigFilePath("config.xml");
}

} // namespace Utils
} // namespace Backend
} // namespace CargoNetSim
