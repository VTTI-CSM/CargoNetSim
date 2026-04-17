#include "ShipSimulationClient.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QThread>

#include "Backend/Commons/LogCategories.h"
#include "Backend/Models/ShipSystem.h"
// #include "TerminalGraphServer.h"
// #include "SimulatorTimeServer.h"
// #include "ProgressBarManager.h"
// #include "ApplicationLogger.h"

namespace CargoNetSim
{
namespace Backend
{
namespace ShipClient
{

SimulationSummaryData::SimulationSummaryData(
    const QList<SummaryPair> &summaryData)
    : m_rawSummaryData(summaryData)
{
    qCDebug(lcClientShip) << "SimulationSummaryData::SimulationSummaryData:"
                          << "rawPairs=" << summaryData.size();
    m_parsedData = _parseSummaryData();
    qCInfo(lcClientShip) << "SimulationSummaryData::SimulationSummaryData:"
                         << "parsed categories=" << m_parsedData.keys().size();
}

QVariantMap SimulationSummaryData::_parseSummaryData()
{
    qCDebug(lcClientShip) << "SimulationSummaryData::_parseSummaryData:"
                          << "parsing" << m_rawSummaryData.size() << "raw pairs";

    QVariantMap parsed;
    QString     currentCategory;
    QString     currentSubcategory;

    for (const auto &pair : m_rawSummaryData)
    {
        QString summaryText = pair.first.trimmed();
        QString value       = pair.second;

        // Skip separator lines
        if (summaryText.isEmpty()
            || summaryText.startsWith("~.~")
            || summaryText.startsWith("..."))
        {
            continue;
        }

        // Handle new categories
        if (summaryText.startsWith('+'))
        {
            currentCategory = summaryText.mid(1).trimmed();
            currentCategory.remove(':');
            parsed[currentCategory] = QVariantMap();
            currentSubcategory.clear();
            qCDebug(lcClientShip) << "SimulationSummaryData::_parseSummaryData:"
                                  << "new category=" << currentCategory;
            continue;
        }

        // Handle new subcategories
        if (summaryText.startsWith("|->"))
        {
            currentSubcategory =
                summaryText.mid(3).trimmed();
            if (!currentCategory.isEmpty())
            {
                QVariantMap categoryMap =
                    parsed[currentCategory].toMap();
                categoryMap[currentSubcategory] =
                    QVariantMap();
                parsed[currentCategory] = categoryMap;
                qCDebug(lcClientShip) << "SimulationSummaryData::_parseSummaryData:"
                                      << "new subcategory=" << currentSubcategory
                                      << "in category=" << currentCategory;
            }
            continue;
        }

        // Handle key-value pairs
        if (summaryText.startsWith("|_"))
        {
            QString key = summaryText.mid(2).trimmed();
            if (!currentCategory.isEmpty())
            {
                QVariantMap categoryMap =
                    parsed[currentCategory].toMap();

                if (!currentSubcategory.isEmpty())
                {
                    QVariantMap subcategoryMap =
                        categoryMap[currentSubcategory]
                            .toMap();
                    subcategoryMap[key] = value;
                    categoryMap[currentSubcategory] =
                        subcategoryMap;
                }
                else
                {
                    categoryMap[key] = value;
                }

                parsed[currentCategory] = categoryMap;
            }
        }
    }

    qCDebug(lcClientShip) << "SimulationSummaryData::_parseSummaryData:"
                          << "result categories=" << parsed.keys();
    return parsed;
}

QMap<QString, QVariant> SimulationSummaryData::getCategory(
    const QString &category) const
{
    qCDebug(lcClientShip) << "SimulationSummaryData::getCategory:" << category;
    auto result = m_parsedData.value(category).toMap();
    if (result.isEmpty())
    {
        qCWarning(lcClientShip) << "SimulationSummaryData::getCategory:"
                                << "category not found:" << category;
    }
    return result;
}

QMap<QString, QVariant>
SimulationSummaryData::getSubcategory(
    const QString &category,
    const QString &subcategory) const
{
    qCDebug(lcClientShip) << "SimulationSummaryData::getSubcategory:"
                          << "category=" << category
                          << "subcategory=" << subcategory;
    QVariantMap categoryData = getCategory(category);
    auto result = categoryData.value(subcategory).toMap();
    if (result.isEmpty())
    {
        qCWarning(lcClientShip) << "SimulationSummaryData::getSubcategory:"
                                << "subcategory not found:" << subcategory
                                << "in category:" << category;
    }
    return result;
}

QVariant
SimulationSummaryData::getValue(const QString &category,
                                const QString &subcategory,
                                const QString &key) const
{
    qCDebug(lcClientShip) << "SimulationSummaryData::getValue:"
                          << "category=" << category
                          << "subcategory=" << subcategory
                          << "key=" << key;
    QVariantMap subcategoryData =
        getSubcategory(category, subcategory);
    QVariant result = subcategoryData.value(key);
    if (!result.isValid())
    {
        qCWarning(lcClientShip) << "SimulationSummaryData::getValue:"
                                << "key not found:" << key;
    }
    return result;
}

QStringList SimulationSummaryData::getAllCategories() const
{
    QStringList cats = m_parsedData.keys();
    qCDebug(lcClientShip) << "SimulationSummaryData::getAllCategories:"
                          << "count=" << cats.size() << cats;
    return cats;
}

QMap<QString, QStringList>
SimulationSummaryData::getAllSubcategories(
    const QString &category) const
{
    qCDebug(lcClientShip) << "SimulationSummaryData::getAllSubcategories:"
                          << "category=" << category;
    QMap<QString, QStringList> result;

    if (category == "*")
    {
        for (auto it = m_parsedData.constBegin();
             it != m_parsedData.constEnd(); ++it)
        {
            QVariantMap categoryData = it.value().toMap();
            result[it.key()]         = categoryData.keys();
        }
    }
    else
    {
        QVariantMap categoryData =
            m_parsedData.value(category).toMap();
        result[category] = categoryData.keys();
    }

    qCDebug(lcClientShip) << "SimulationSummaryData::getAllSubcategories:"
                          << "result keys=" << result.keys();
    return result;
}

QVariantMap SimulationSummaryData::info() const
{
    qCDebug(lcClientShip) << "SimulationSummaryData::info:"
                          << "categories=" << m_parsedData.keys().size();
    return m_parsedData;
}

} // namespace ShipClient
} // namespace Backend
} // namespace CargoNetSim
