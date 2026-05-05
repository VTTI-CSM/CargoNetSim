#pragma once

#include <QGraphicsObject>
#include <QPointer>

#include "Backend/Commons/TransportationMode.h"
#include "../Interfaces/IInteractionMode.h"

namespace CargoNetSim::GUI {
class GlobalTerminalItem;
class TerminalItem;
namespace Input {

class ConnectMode final : public IInteractionMode {
public:
    using Mode = Backend::TransportationTypes::TransportationMode;

    explicit ConnectMode(Mode type);
    ~ConnectMode() override;

    QString name() const override { return QStringLiteral("Connect"); }
    QCursor cursor() const override { return QCursor(Qt::CrossCursor); }

    Handled onPress   (const PressEvent&,    const ClickContext&) override;
    Handled onKeyPress(const KeyPressEvent&, const ClickContext&) override;

    void onEnter(const ClickContext&) override;
    void onExit (const ClickContext&) override;

    void setConnectionType(Mode t) { m_connectionType = t; }
    Mode connectionType() const { return m_connectionType; }

private:
    // Holds either a TerminalItem or a GlobalTerminalItem (both are QGraphicsObject).
    QPointer<QGraphicsObject> m_firstItem;
    Mode                      m_connectionType;
};

} // namespace Input
} // namespace CargoNetSim::GUI
