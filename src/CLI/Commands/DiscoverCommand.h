#pragma once

#include "CLI/Subcommand.h"

class QIODevice;

namespace CargoNetSim {
namespace Cli {

class DiscoverCommand : public Subcommand
{
public:
    explicit DiscoverCommand(QIODevice *outSink = nullptr,
                             QIODevice *errSink = nullptr);

    int execute(const QStringList &args) override;

private:
    QIODevice *m_out;
    QIODevice *m_err;
};

} // namespace Cli
} // namespace CargoNetSim
