#include "CsvResultsWriter.h"
#include "Backend/Commons/LogCategories.h"

#include <QSaveFile>
#include <QStringConverter>
#include <QTextStream>

namespace CargoNetSim {
namespace Cli {

bool CsvResultsWriter::write(
    const QString &outputPath,
    const QList<Backend::Scenario::PathSimulationResult> &results,
    QString *err)
{
    qCInfo(lcCli) << "CsvResultsWriter::write: path" << outputPath
                  << "rows:" << results.size();
    // Binary mode: no QIODevice::Text flag. The Text flag enables
    // platform line-ending translation (Windows: \n → \r\n) which
    // would violate spec §9 "Line endings: LF everywhere". QTextStream
    // still emits `\n` literally when we write `QChar('\n')` — binary
    // mode just keeps Qt from rewriting it.
    QSaveFile f(outputPath);
    if (!f.open(QIODevice::WriteOnly))
    {
        qCWarning(lcCli) << "CsvResultsWriter::write: cannot open" << outputPath
                         << f.errorString();
        if (err)
            *err = QStringLiteral("Cannot open %1: %2")
                       .arg(outputPath, f.errorString());
        return false;
    }

    QTextStream ts(&f);
    ts.setEncoding(QStringConverter::Utf8);
    ts.setGenerateByteOrderMark(false);

    const QChar LF(QLatin1Char('\n'));

    ts << QStringLiteral("path_id,total_cost,edge_costs,terminal_costs")
       << LF;
    for (const auto &r : results)
    {
        ts << r.pathId << QLatin1Char(',')
           << QString::number(r.totalCost,     'f', 6) << QLatin1Char(',')
           << QString::number(r.edgeCosts,     'f', 6) << QLatin1Char(',')
           << QString::number(r.terminalCosts, 'f', 6) << LF;
    }

    // QTextStream buffers internally; flush before committing so the
    // final bytes land in the QSaveFile staging buffer.
    ts.flush();
    if (!f.commit())
    {
        qCWarning(lcCli) << "CsvResultsWriter::write: cannot commit" << outputPath
                         << f.errorString();
        if (err)
            *err = QStringLiteral("Cannot commit %1: %2")
                       .arg(outputPath, f.errorString());
        return false;
    }
    qCInfo(lcCli) << "CsvResultsWriter::write: wrote" << results.size()
                  << "rows to" << outputPath;
    return true;
}

} // namespace Cli
} // namespace CargoNetSim
