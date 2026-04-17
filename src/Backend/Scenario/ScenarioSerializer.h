#pragma once

#include "ScenarioDocument.h"
#include <QJsonObject>
#include <QString>
#include <memory>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

/// Translates between ScenarioDocument and YAML / JSON on-disk forms.
///
/// YAML is canonical (human-authored). JSON is the same schema expressed
/// in a machine-friendly format; GUI and CLI both use toJson when producing
/// preview / CI-diff output.
///
/// Stateless — all methods are static.
class ScenarioSerializer
{
public:
    /// Current supported schema version. Unknown versions are rejected.
    static constexpr int kSchemaVersion = 1;

    // JSON
    static QJsonObject                        toJson(const ScenarioDocument &doc);
    static std::unique_ptr<ScenarioDocument>  fromJson(const QJsonObject &j);

    // YAML (Task 13).
    // Paths inside YAML are resolved relative to the file's directory after
    // QDir::fromNativeSeparators().
    static bool                               toYaml(const ScenarioDocument &doc,
                                                    const QString &path,
                                                    QString *error = nullptr);
    static std::unique_ptr<ScenarioDocument>  fromYaml(const QString &path,
                                                      QString *error = nullptr);
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
