#include "SimulationSummaryData.h"

/**
 * @file SimulationSummaryData.cpp
 * @brief Implementation file for the SimulationSummaryData
 * class
 * @author Ahmed Aredah
 * @date March 20, 2025
 *
 * This file contains the implementation of the
 * SimulationSummaryData class, which parses and manages
 * simulation summary data in a hierarchical structure.
 * Extensive comments are included to aid in auditing and
 * reviewing the code.
 */

namespace CargoNetSim
{
namespace Backend
{
namespace TrainClient
{

SimulationSummaryData::SimulationSummaryData(
    const QVector<QPair<QString, QString>> &summaryData)
{
    // Upon construction, immediately parse the provided
    // summary data to initialize the internal data
    // structure (m_parsedData).
    parseSummaryData(summaryData);
}

void SimulationSummaryData::parseSummaryData(
    const QVector<QPair<QString, QString>> &summaryData)
{
    // Temporary map to build the parsed structure before
    // assigning to m_parsedData
    QMap<QString, QVariant> parsed;

    // Pointers to track the current category and
    // subcategory maps during parsing
    QMap<QString, QVariant> *currentCategoryMap =
        nullptr; // Tracks the current category
    QMap<QString, QVariant> *currentSubcategoryMap =
        nullptr; // Tracks the current subcategory

    // Iterate over each text-value pair in the input
    // summary data
    for (const auto &pair : summaryData)
    {
        // Extract and clean the summary text (remove
        // leading/trailing whitespace)
        QString text = pair.first.trimmed();
        // Extract the value associated with the text (may
        // be empty)
        QString value = pair.second;

        // Skip irrelevant or formatting lines
        if (text.isEmpty() || text.startsWith("~.~")
            || text.startsWith("..."))
        {
            continue; // These are separators or empty
                      // lines, ignore them
        }

        // Handle new category (lines starting with '+')
        if (text.startsWith("+"))
        {
            // Extract category name by removing the '+'
            // prefix and trimming
            QString category = text.mid(1).trimmed();
            // Remove trailing colon if present (for
            // consistency)
            category = category.remove(":");
            // Initialize an empty map for this category
            parsed[category] = QMap<QString, QVariant>();
            // Get the map out of it
            auto results = parsed[category].toMap();
            // Point currentCategoryMap to this new
            // category's map
            currentCategoryMap = &results;
            // Reset subcategory pointer since we're at a
            // new category
            currentSubcategoryMap = nullptr;
            continue;
        }

        // Handle new subcategory (lines starting with
        // '|->')
        if (text.startsWith("|->"))
        {
            // Ensure we have a current category to attach
            // the subcategory to
            if (currentCategoryMap)
            {
                // Extract subcategory name by removing
                // '|->' and trimming
                QString subcategory = text.mid(3).trimmed();
                // Initialize an empty map for this
                // subcategory
                (*currentCategoryMap)[subcategory] =
                    QMap<QString, QVariant>();
                // Get the map out of it
                auto result =
                    (*currentCategoryMap)[subcategory]
                        .toMap();
                // Point currentSubcategoryMap to this new
                // subcategory's map
                currentSubcategoryMap = &result;
            }
            continue;
        }

        // Handle key-value pairs (lines starting with '|_')
        if (text.startsWith("|_"))
        {
            // Extract the key by removing '|_ ' prefix and
            // trimming
            QString key = text.mid(2).trimmed();
            // Determine where to store the key-value pair
            if (currentSubcategoryMap)
            {
                // If a subcategory is active, store the
                // key-value pair there
                (*currentSubcategoryMap)[key] = value;
            }
            else if (currentCategoryMap)
            {
                // If no subcategory, store directly in the
                // current category
                (*currentCategoryMap)[key] = value;
            }
            // If neither pointer is set, the data is
            // malformed; ignore it
            continue;
        }
    }

    // Assign the fully constructed structure to the member
    // variable
    m_parsedData = parsed;
}

QMap<QString, QVariant> SimulationSummaryData::getCategory(
    const QString &category) const
{
    // Retrieve the map associated with the specified
    // category If the category doesn't exist, returns an
    // empty QMap
    return m_parsedData.value(category).toMap();
}

QMap<QString, QVariant>
SimulationSummaryData::getSubcategory(
    const QString &category,
    const QString &subcategory) const
{
    // First, get the category map
    QMap<QString, QVariant> catData = getCategory(category);
    // Then, retrieve the subcategory map from within the
    // category Returns an empty QMap if the subcategory
    // doesn't exist
    return catData.value(subcategory).toMap();
}

QVariant
SimulationSummaryData::getValue(const QString &category,
                                const QString &subcategory,
                                const QString &key) const
{
    // Get the subcategory map
    QMap<QString, QVariant> subcatData =
        getSubcategory(category, subcategory);
    // Return the value associated with the key
    // Returns an invalid QVariant if the key doesn't exist
    return subcatData.value(key);
}

QStringList SimulationSummaryData::getAllCategories() const
{
    // Simply return a list of all top-level keys (category
    // names) in m_parsedData
    return m_parsedData.keys();
}

QMap<QString, QStringList>
SimulationSummaryData::getAllSubcategories(
    const QString &category) const
{
    // Map to store the result: category names mapped to
    // their subcategory lists
    QMap<QString, QStringList> result;

    if (category == "*")
    {
        // Special case: return subcategories for all
        // categories
        for (const QString &cat : m_parsedData.keys())
        {
            QMap<QString, QVariant> catData =
                m_parsedData[cat].toMap();
            QStringList subcats;
            // Iterate through the category's keys
            for (const QString &key : catData.keys())
            {
                // If the value is a map, it's a subcategory
                if (catData[key].typeId() == QMetaType::QVariantMap)
                {
                    subcats.append(key);
                }
            }
            result[cat] = subcats;
        }
    }
    else
    {
        // Specific case: return subcategories for the given
        // category
        QMap<QString, QVariant> catData =
            getCategory(category);
        QStringList subcats;
        for (const QString &key : catData.keys())
        {
            if (catData[key].typeId() == QMetaType::QVariantMap)
            {
                subcats.append(key);
            }
        }
        result[category] = subcats;
    }
    return result;
}

QMap<QString, QVariant> SimulationSummaryData::info() const
{
    // Return a copy of the entire parsed data structure
    // Useful for debugging or full data inspection
    return m_parsedData;
}

} // namespace TrainClient
} // namespace Backend
} // namespace CargoNetSim
