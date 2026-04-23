#include "RuntimeArtifactIdentity.h"

#include "Backend/Models/Path.h"

#include <QUrl>

namespace CargoNetSim {
namespace Backend {
namespace Scenario {
namespace {

QString escapePart(const QString &value)
{
    return QString::fromUtf8(
        QUrl::toPercentEncoding(value, QByteArray(), QByteArray("|=%")));
}

QString unescapePart(const QString &value)
{
    return QUrl::fromPercentEncoding(value.toUtf8());
}

} // namespace

namespace RuntimeArtifacts {

QString encode(const RuntimeArtifactIdentity &id)
{
    return QStringLiteral("ra|type=%1|path=%2|seg=%3|idx=%4")
        .arg(escapePart(id.artifactType), escapePart(id.pathKey),
             QString::number(id.segmentIndex),
             QString::number(id.artifactIndex));
}

bool decode(const QString &encoded, RuntimeArtifactIdentity &out)
{
    if (!encoded.startsWith(QStringLiteral("ra|")))
        return false;

    RuntimeArtifactIdentity parsed;
    const QStringList parts =
        encoded.mid(3).split(QLatin1Char('|'), Qt::SkipEmptyParts);
    for (const QString &part : parts)
    {
        const int eq = part.indexOf(QLatin1Char('='));
        if (eq <= 0)
            return false;
        const QString key = part.left(eq);
        const QString value = part.mid(eq + 1);
        if (key == QStringLiteral("type"))
            parsed.artifactType = unescapePart(value);
        else if (key == QStringLiteral("path"))
            parsed.pathKey = unescapePart(value);
        else if (key == QStringLiteral("seg"))
            parsed.segmentIndex = value.toInt();
        else if (key == QStringLiteral("idx"))
            parsed.artifactIndex = value.toInt();
    }

    if (!parsed.isValid())
        return false;

    out = parsed;
    return true;
}

QString vehicleId(const Backend::Path *path, int segmentIndex,
                  int artifactIndex, const QString &artifactType)
{
    RuntimeArtifactIdentity id;
    id.pathKey = path ? path->canonicalPathKey() : QString();
    id.segmentIndex = segmentIndex;
    id.artifactIndex = artifactIndex;
    id.artifactType = artifactType;
    return encode(id);
}

QString copiedContainerId(const Backend::Path *path, int segmentIndex,
                          int artifactIndex,
                          const QString &sourceContainerId)
{
    return QStringLiteral("%1|src=%2")
        .arg(vehicleId(path, segmentIndex, artifactIndex,
                       QStringLiteral("container")),
             escapePart(sourceContainerId));
}

} // namespace RuntimeArtifacts
} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
