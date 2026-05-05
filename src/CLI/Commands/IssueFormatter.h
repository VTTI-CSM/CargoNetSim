#pragma once

#include <QLatin1String>
#include <QList>
#include <QLocale>
#include <QMap>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

#include "Backend/CliApi/ValidationApi.h"

namespace CargoNetSim {
namespace Cli {

struct ValidationIssueFormatOptions
{
    bool allErrors = false;
    int  summaryThreshold = 25;
    int  maxExamplesPerGroup = 3;
};

namespace Detail {

inline const char *severityLabel(
    Backend::Scenario::ValidationIssue::Severity severity)
{
    using Sev = Backend::Scenario::ValidationIssue::Severity;
    return severity == Sev::Error   ? "ERROR  "
         : severity == Sev::Warning ? "WARN   "
                                    : "INFO   ";
}

inline QString normalizedIssuePath(const QString &path)
{
    static const QRegularExpression numericIndexPattern(
        QStringLiteral("\\[[0-9]+\\]"));
    if (path.isEmpty())
        return QStringLiteral("<document>");

    QString normalized = path;
    normalized.replace(numericIndexPattern, QStringLiteral("[]"));
    return normalized;
}

inline QString formatIssueLine(
    const Backend::Scenario::ValidationIssue &issue)
{
    QString line = QLatin1String(severityLabel(issue.severity));
    line += issue.path;
    line += QStringLiteral(": ");
    line += issue.message;
    line += QLatin1Char('\n');
    return line;
}

struct IssueGroup
{
    Backend::Scenario::ValidationIssue::Severity severity =
        Backend::Scenario::ValidationIssue::Error;
    QString normalizedPath;
    QString message;
    QString firstPath;
    QString lastPath;
    QStringList examples;
    int count = 0;
};

inline QString issueCountText(int count)
{
    return QLocale(QLocale::English, QLocale::UnitedStates)
        .toString(count);
}

inline QString groupedIssueKey(
    const Backend::Scenario::ValidationIssue &issue)
{
    return QString::number(static_cast<int>(issue.severity))
           + QLatin1Char('\x1f')
           + normalizedIssuePath(issue.path)
           + QLatin1Char('\x1f')
           + issue.message;
}

} // namespace Detail

/**
 * @brief Format a list of `ValidationIssue`s into a stable
 *        human-readable multi-line block for stderr.
 *
 * Extracted so every command that surfaces validator output (`validate`,
 * `preview`, `discover`, and `run` preflight) uses the same policy. Small
 * issue sets are emitted one issue per line; large issue sets are grouped
 * unless callers pass `ValidationIssueFormatOptions::allErrors`.
 *
 * Full-output line format per issue (fixed-width severity prefix + dotted
 * path + message + LF):
 * @code
 *   ERROR  terminals[T1].type: <message>
 *   WARN   simulation.dwell_time.parameters: <message>
 * @endcode
 * Width 7 for the severity column keeps subsequent columns aligned
 * on mixed-severity runs.
 *
 * @param issues   The issues to format (may be empty → returns "").
 * @param anyError Optional out-param: set to true iff at least one
 *                 issue has `Severity::Error`. Callers use this to
 *                 distinguish "warnings only" (exit 0 possible) from
 *                 "hard error present" (exit ≠ 0) without re-walking
 *                 the list.
 */
inline QString formatValidationIssues(
    const QList<Backend::Scenario::ValidationIssue> &issues,
    bool *anyError = nullptr,
    ValidationIssueFormatOptions options =
        ValidationIssueFormatOptions())
{
    using Sev = Backend::Scenario::ValidationIssue::Severity;
    if (anyError) *anyError = false;

    int errorCount = 0;
    int warningCount = 0;
    for (const auto &iss : issues)
    {
        if (iss.severity == Sev::Error)
        {
            ++errorCount;
            if (anyError)
                *anyError = true;
        }
        else if (iss.severity == Sev::Warning)
        {
            ++warningCount;
        }
    }

    QString buffer;
    if (options.allErrors
        || issues.size() <= options.summaryThreshold)
    {
        for (const auto &iss : issues)
            buffer += Detail::formatIssueLine(iss);
        return buffer;
    }

    const QString header = errorCount > 0
        ? QStringLiteral("Validation failed")
        : QStringLiteral("Validation warnings");
    buffer += QStringLiteral("%1: %2 error(s), %3 warning(s)\n\n")
                  .arg(header,
                       Detail::issueCountText(errorCount),
                       Detail::issueCountText(warningCount));
    buffer += QStringLiteral("Grouped validation issues:\n");

    QList<Detail::IssueGroup> groups;
    QMap<QString, int> groupIndexByKey;
    for (const auto &issue : issues)
    {
        const QString key = Detail::groupedIssueKey(issue);
        int groupIndex = groupIndexByKey.value(key, -1);
        if (groupIndex < 0)
        {
            Detail::IssueGroup group;
            group.severity = issue.severity;
            group.normalizedPath =
                Detail::normalizedIssuePath(issue.path);
            group.message = issue.message;
            group.firstPath = issue.path;
            groupIndex = groups.size();
            groupIndexByKey.insert(key, groupIndex);
            groups.append(group);
        }

        auto &group = groups[groupIndex];
        ++group.count;
        group.lastPath = issue.path;
        if (group.examples.size() < options.maxExamplesPerGroup)
            group.examples.append(issue.path);
    }

    for (const auto &group : groups)
    {
        buffer += QStringLiteral("  %1x %2%3: %4\n")
                      .arg(Detail::issueCountText(group.count),
                           QLatin1String(
                               Detail::severityLabel(group.severity)),
                           group.normalizedPath,
                           group.message);

        if (!group.examples.isEmpty())
        {
            buffer += QStringLiteral("     examples: %1\n")
                          .arg(group.examples.join(
                              QStringLiteral(", ")));
        }

        if (group.count > 1 && group.firstPath != group.lastPath)
        {
            buffer += QStringLiteral("     range: %1 .. %2\n")
                          .arg(group.firstPath, group.lastPath);
        }
    }

    buffer += QStringLiteral(
        "\nUse --all-errors to print every validation issue.\n");
    return buffer;
}

inline ValidationIssueFormatOptions validationIssueFormatOptions(
    bool allErrors)
{
    ValidationIssueFormatOptions options;
    options.allErrors = allErrors;
    return options;
}

} // namespace Cli
} // namespace CargoNetSim
