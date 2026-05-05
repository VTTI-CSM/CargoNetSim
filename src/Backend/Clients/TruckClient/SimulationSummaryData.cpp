/**
 * @file SimulationSummaryData.cpp
 * @brief Implements truck simulation summary data
 * @author Ahmed Aredah
 * @date March 21, 2025
 */

#include "SimulationSummaryData.h"

#include "Backend/Commons/LogCategories.h"

namespace CargoNetSim
{
namespace Backend
{
namespace TruckClient
{

SimulationSummaryData::SimulationSummaryData(
    const QList<SummaryPair> &summaryData, QObject *parent)
    : QObject(parent)
    , m_rawSummaryData(summaryData)
{
    qCDebug(lcClientTruck) << "SimulationSummaryData::SimulationSummaryData:"
                           << "rawPairs=" << summaryData.size();
    m_parsedData = parseSummaryData();
    qCInfo(lcClientTruck) << "SimulationSummaryData::SimulationSummaryData:"
                          << "parsed categories=" << m_parsedData.keys().size();
}

QMap<QString, QVariant> SimulationSummaryData::getCategory(
    const QString &category) const
{
    qCDebug(lcClientTruck) << "SimulationSummaryData::getCategory:" << category;
    auto result = m_parsedData.value(category).toMap();
    if (result.isEmpty())
    {
        qCWarning(lcClientTruck) << "SimulationSummaryData::getCategory:"
                                 << "category not found:" << category;
    }
    return result;
}

QMap<QString, QVariant>
SimulationSummaryData::getSubcategory(
    const QString &category,
    const QString &subcategory) const
{
    qCDebug(lcClientTruck) << "SimulationSummaryData::getSubcategory:"
                           << "category=" << category
                           << "subcategory=" << subcategory;
    QVariantMap categoryData = getCategory(category);
    auto result = categoryData.value(subcategory).toMap();
    if (result.isEmpty())
    {
        qCWarning(lcClientTruck) << "SimulationSummaryData::getSubcategory:"
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
    qCDebug(lcClientTruck) << "SimulationSummaryData::getValue:"
                           << "category=" << category
                           << "subcategory=" << subcategory
                           << "key=" << key;
    QVariantMap subcategoryData =
        getSubcategory(category, subcategory);
    QVariant result = subcategoryData.value(key);
    if (!result.isValid())
    {
        qCWarning(lcClientTruck) << "SimulationSummaryData::getValue:"
                                 << "key not found:" << key;
    }
    return result;
}

QStringList SimulationSummaryData::getAllCategories() const
{
    QStringList cats = m_parsedData.keys();
    qCDebug(lcClientTruck) << "SimulationSummaryData::getAllCategories:"
                           << "count=" << cats.size() << cats;
    return cats;
}

QMap<QString, QStringList>
SimulationSummaryData::getAllSubcategories(
    const QString &category) const
{
    qCDebug(lcClientTruck) << "SimulationSummaryData::getAllSubcategories:"
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

    qCDebug(lcClientTruck) << "SimulationSummaryData::getAllSubcategories:"
                           << "result keys=" << result.keys();
    return result;
}

QVariantMap SimulationSummaryData::info() const
{
    qCDebug(lcClientTruck) << "SimulationSummaryData::info:"
                           << "categories=" << m_parsedData.keys().size();
    return m_parsedData;
}

QVariantMap SimulationSummaryData::parseSummaryData()
{
    qCDebug(lcClientTruck) << "SimulationSummaryData::parseSummaryData:"
                           << "parsing" << m_rawSummaryData.size() << "raw pairs";

    QVariantMap parsed;
    QString     currentCategory;
    QString     currentSubcategory;

    for (const auto &pair : m_rawSummaryData)
    {
        QString summaryText = pair.first.trimmed();
        QString value       = pair.second;

        if (summaryText.isEmpty()
            || summaryText.startsWith("~.~")
            || summaryText.startsWith("..."))
        {
            continue;
        }

        if (summaryText.startsWith('+'))
        {
            currentCategory = summaryText.mid(1).trimmed();
            currentCategory.remove(':');
            parsed[currentCategory] = QVariantMap();
            currentSubcategory.clear();
            qCDebug(lcClientTruck) << "SimulationSummaryData::parseSummaryData:"
                                   << "new category=" << currentCategory;
            continue;
        }

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
                qCDebug(lcClientTruck) << "SimulationSummaryData::parseSummaryData:"
                                       << "new subcategory=" << currentSubcategory
                                       << "in category=" << currentCategory;
            }
            continue;
        }

        if (summaryText.startsWith("|_"))
        {
            QString key = summaryText.mid(2).trimmed();
            if (!currentCategory.isEmpty())
            {
                QVariantMap categoryMap =
                    parsed[currentCategory].toMap();
                if (!currentSubcategory.isEmpty())
                {
                    QVariantMap subMap =
                        categoryMap[currentSubcategory]
                            .toMap();
                    subMap[key] = value;
                    categoryMap[currentSubcategory] =
                        subMap;
                }
                else
                {
                    categoryMap[key] = value;
                }
                parsed[currentCategory] = categoryMap;
            }
        }
    }

    qCDebug(lcClientTruck) << "SimulationSummaryData::parseSummaryData:"
                           << "result categories=" << parsed.keys();
    return parsed;
}

} // namespace TruckClient
} // namespace Backend
} // namespace CargoNetSim
